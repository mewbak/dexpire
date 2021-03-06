/*
 *
 * Dexpire - mainwindows.cpp
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE.txt', which is part of this source code package.
 *
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDesktopWidget>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardItemModel>
#include <cDexFile.h>
#include <QSplitter>
#include <QTreeView>
#include <QTextEdit>
#include "treemodel.h"
#include <QDirModel>
#include <cDexDecompiler.h>
#include <QLabel>
#include <iostream>
#include <QTableView>
#include <QByteArray>
#include <cDexString.h>
#include "codeeditor.h"
#include "processthread.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    uiCenterScreen();
    uiSetupToolbar();
    uiSetupSplitter();
    uiSetupWorkspace();

    resizeEvent(NULL);

    dexFile = NULL;
    dexDecompiler = NULL;
    apkFile = NULL;

    cleanCurrentWorkspace();

    re = new QRegExp("\\d*");

    //dexFile = new cDexFile("H:/Projects/dexpire/test/classes.dex");
    //delete dexFile;
    //dexFile = NULL;
    //prepareDexWorkspace();
}

void MainWindow::uiSetupWorkspace()
{
    treeModel = NULL;
    stringsTab = NULL;
    methodsTab = NULL;
    fieldsTab = NULL;
    typesTab = NULL;
    headerTab = NULL;
    protoTab = NULL;

    ui->treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->treeView->setSelectionBehavior(QAbstractItemView::SelectItems);

    treeViewSignalsRegistered = false;

    ui->tabWidget->tabBar()->installEventFilter(new TabWidgetEventFilter(ui->tabWidget->tabBar()));
}

void MainWindow::uiSetupSplitter()
{
    QList<int>* sizesList = new QList<int>();
    sizesList->append(180);
    sizesList->append(1);
    ui->splitter->setSizes(*sizesList);
    delete sizesList;

    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setCollapsible(0, false);
    ui->splitter->setCollapsible(1, false);
}

void MainWindow::loadFileDialog()
{
    QString Filename = QFileDialog::getOpenFileName(
                this, "Open File","classes.dex", "DEX File (*.dex);;APK File (*.apk)");

    if (!Filename.isEmpty())
    {
        if (Filename.endsWith(".dex") || Filename.endsWith(".apk"))
        {
            cleanCurrentWorkspace();

            ProcessThread* thread = new ProcessThread
                    (&dexFile,
                     &dexDecompiler,
                     Filename.toLocal8Bit().data(),
                     &treeModel,
                     Filename.endsWith(".apk")? &apkFile: NULL);

            connect(thread, SIGNAL(finished()), this, SLOT(with_processThread_finished()));

            ui->statusBar->showMessage(QString("Parsing ").append(Filename).append(" ..."));
            thread->start();
        }
    }
}

void MainWindow::with_processThread_finished()
{
    if (!dexFile->isReady)
        QMessageBox::critical(this, "Error", "Unable to process the specified file", QMessageBox::Ok);
    else
    {
        prepareDexWorkspace();

        if (apkFile)
            prepareApkWorkspace();     
    }
}

void MainWindow::prepareApkWorkspace()
{

}

void MainWindow::cleanCurrentWorkspace()
{
    if (treeModel)
    {
        delete treeModel;
        treeModel = NULL;
    }

    if (dexDecompiler)
    {
        delete dexDecompiler;
        dexDecompiler = NULL;
    }

    if (dexFile)
    {
        delete dexFile;
        dexFile = NULL;
    }

    if (apkFile)
    {
        delete apkFile;
        apkFile = NULL;
    }

    ui->tabWidget->clear();
    ui->statusBar->clearMessage();

    setClassToolbarEnabled(false);
    setDexToolbarEnabled(false);
    setApkToolbarEnabled(false);
}

void MainWindow::prepareDexWorkspace()
{
    ui->treeView->setModel(treeModel);
    ui->treeView->expandToDepth(0);

    setClassToolbarEnabled(false);
    setDexToolbarEnabled(true);

    ui->statusBar->showMessage(QString(apkFile? apkFile->Filename: dexFile->Filename).append(" loaded successfully."));

    if (!treeViewSignalsRegistered)
    {
        treeViewSignalsRegistered = true;
        connect(ui->treeView, SIGNAL(collapsed(QModelIndex)), this, SLOT(with_treeView_collapsed(QModelIndex)));
        connect(ui->treeView, SIGNAL(expanded(QModelIndex)), this, SLOT(with_treeView_expanded(QModelIndex)));
    }
}

void MainWindow::uiSetupToolbar()
{
    toolbar = ui->mainToolBar;
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);

    QMenu *menu = new QMenu("Tables");
    menu->setIcon(QIcon(":/icons/properties.gif"));
    menu->addAction(ui->actionFields_Table_2);
    menu->addAction(ui->actionStrings_Table_2);
    menu->addAction(ui->actionMethods_Table_2);
    menu->addAction(ui->actionTypes_Table_2);
    menu->addAction(ui->actionPrototypes_Table_2);
    toolbar->addAction(menu->menuAction());
}

void MainWindow::resizeEvent(QResizeEvent*)
{
    ui->splitter->resize(
                geometry().width() - 10,
                geometry().height() - ui->mainToolBar->geometry().height() - 50);
}

void MainWindow::uiCenterScreen()
{
    QRect frect = frameGeometry();
    frect.moveCenter(QDesktopWidget().availableGeometry().center());
    move(frect.topLeft());
}

MainWindow::~MainWindow()
{
    cleanCurrentWorkspace();
    delete ui;
}

void MainWindow::on_actionFields_Table_triggered()
{
    int index = tabOpened(QString("Fields Table"));
    if (index != -1)
        ui->tabWidget->setCurrentIndex(index);
    else
    {
        if (!fieldsTab)
        {
            fieldsTab = new QTableView(this);
            fieldsTab->setWordWrap(true);
            fieldsTab->setSelectionMode(QAbstractItemView::SingleSelection);
            fieldsTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
            fieldsTab->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
            fieldsTab->verticalHeader()->setDefaultSectionSize(22);
            fieldsTab->setSelectionBehavior(QAbstractItemView::SelectRows);

            QObject::connect(fieldsTab, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(with_fieldsTab_doubleClicked(QModelIndex)));

            QStandardItemModel *model = new QStandardItemModel(dexFile->nFieldIDs, 3, this);
            model->setHorizontalHeaderItem(0, new QStandardItem(QString("Class Index")));
            model->setHorizontalHeaderItem(1, new QStandardItem(QString("Type Index")));
            model->setHorizontalHeaderItem(2, new QStandardItem(QString("Name")));

            for (unsigned int i=0; i<dexFile->nFieldIDs; i++)
            {
                model->setItem(i, 0, new QStandardItem(QString().sprintf("0x%04x", dexFile->DexFieldIds[i].ClassIndex)));
                model->setItem(i, 1, new QStandardItem(QString().sprintf("0x%04x", dexFile->DexFieldIds[i].TypeIdex)));
                model->setItem(i, 2, new QStandardItem(QString((char*)dexFile->StringItems[dexFile->DexFieldIds[i].StringIndex].Data)));
                model->setVerticalHeaderItem(i, new QStandardItem(QString().sprintf("0x%04x", i)));
            }

            fieldsTab->setModel(model);
            fieldsTab->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        }

        ui->tabWidget->insertTab(0, fieldsTab, QString("Fields Table"));
        ui->tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::with_fieldsTab_doubleClicked(const QModelIndex &index)
{
    switch(index.column())
    {
    case 0:
        on_actionTypes_Table_triggered();
        typesTab->selectRow(dexFile->DexFieldIds[index.row()].ClassIndex);
        break;

    case 1:
        on_actionTypes_Table_triggered();
        typesTab->selectRow(dexFile->DexFieldIds[index.row()].TypeIdex);
        break;

    case 2:
        on_actionStrings_Table_triggered();
        stringsTab->selectRow(dexFile->DexFieldIds[index.row()].StringIndex);
        break;
    }
}

void MainWindow::on_actionOpen_triggered()
{
    loadFileDialog();
}

void MainWindow::on_actionSave_Class_triggered()
{

}

void MainWindow::on_actionSave_All_triggered()
{

}

void MainWindow::on_tabWidget_tabCloseRequested(int index)
{
    ui->tabWidget->removeTab(index);
}

void MainWindow::on_tabWidget_currentChanged(int index)
{

}

void MainWindow::on_actionDex_Disassembly_triggered()
{
    TreeItem* item = ((TreeModel*)(ui->treeView->model()))->getChild( ui->treeView->currentIndex());
    if (item->getType() == TI_CLASS)
        addDexTab(item);
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    TreeItem* item = ((TreeModel*)(ui->treeView->model()))->getChild(index);
    if (item->getType() == TI_CLASS)
        setClassToolbarEnabled(true);
    else
        setClassToolbarEnabled(false);
}

void MainWindow::on_actionStrings_Table_triggered()
{
    int index = tabOpened(QString("Strings Table"));
    if (index != -1)
        ui->tabWidget->setCurrentIndex(index);
    else
    {
        if (!stringsTab)
        {
            stringsTab = new QTableView(this);
            stringsTab->setWordWrap(true);
            stringsTab->setSelectionMode(QAbstractItemView::SingleSelection);
            stringsTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
            stringsTab->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
            stringsTab->verticalHeader()->setDefaultSectionSize(22);
            stringsTab->setSelectionBehavior(QAbstractItemView::SelectRows);

            QStandardItemModel *model = new QStandardItemModel(dexFile->nStringItems, 3, this);
            model->setHorizontalHeaderItem(0, new QStandardItem(QString("Offset")));
            model->setHorizontalHeaderItem(1, new QStandardItem(QString("Size")));
            model->setHorizontalHeaderItem(2, new QStandardItem(QString("String")));

            QStandardItem* item;
            for (unsigned int i=0; i<dexFile->nStringItems; i++)
            {
                model->setItem(i, 0, new QStandardItem(QString().sprintf("0x%08x", (unsigned long)dexFile->StringItems[i].Data - (unsigned long)dexFile->BaseAddress)));
                model->setItem(i, 1, new QStandardItem(QString().sprintf("0x%04x", dexFile->StringItems[i].StringSize)));
                item = new QStandardItem(QString((char*)dexFile->StringItems[i].Data).trimmed());
                item->setToolTip(item->text());
                model->setItem(i, 2, item);
                model->setVerticalHeaderItem(i, new QStandardItem(QString().sprintf("0x%04x", i)));
            }

            stringsTab->setModel(model);
            stringsTab->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        }

        ui->tabWidget->insertTab(0, stringsTab, QString("Strings Table"));
        ui->tabWidget->setCurrentIndex(0);
    }
}

int MainWindow::tabOpened(QString& name)
{
    for (int i=0; i<ui->tabWidget->count(); i++)
        if (ui->tabWidget->tabText(i) == name)
            return i;
    return -1;
}

void MainWindow::on_actionHeader_triggered()
{
    int index = tabOpened(QString("Dex Header"));
    if (index != -1)
        ui->tabWidget->setCurrentIndex(index);
    else
    {
        if (!headerTab)
        {
            headerTab = new QTableView(this);
            headerTab->setWordWrap(true);
            headerTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
            headerTab->setSelectionMode(QAbstractItemView::SingleSelection);
            headerTab->verticalHeader()->hide();
            headerTab->verticalHeader()->setDefaultSectionSize(22);
            headerTab->setSelectionBehavior(QAbstractItemView::SelectRows);

            QStandardItemModel *model = new QStandardItemModel(23, 2, this);
            model->setHorizontalHeaderItem(0, new QStandardItem(QString("Name")));
            model->setHorizontalHeaderItem(1, new QStandardItem(QString("Value")));

            model->setItem(0, 0, new QStandardItem(QString("Magic")));
            model->setItem(0, 1, new QStandardItem(QString((char*)dexFile->DexHeader->Magic)));

            model->setItem(1, 0, new QStandardItem(QString("Checksum")));
            model->setItem(1, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->Checksum)));

            model->setItem(2, 0, new QStandardItem(QString("Signature")));
            model->setItem(2, 1, new QStandardItem(QString(QByteArray::fromRawData((char*)dexFile->DexHeader->Signature, 20).toHex())));

            model->setItem(3, 0, new QStandardItem(QString("FileSize")));
            model->setItem(3, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->FileSize)));

            model->setItem(4, 0, new QStandardItem(QString("HeaderSize")));
            model->setItem(4, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->HeaderSize)));

            model->setItem(5, 0, new QStandardItem(QString("EndianTag")));
            model->setItem(5, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->EndianTag)));

            model->setItem(6, 0, new QStandardItem(QString("LinkSize")));
            model->setItem(6, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->LinkSize)));

            model->setItem(7, 0, new QStandardItem(QString("LinkOffset")));
            model->setItem(7, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->LinkOff)));

            model->setItem(8, 0, new QStandardItem(QString("MapOffset")));
            model->setItem(8, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->MapOff)));

            model->setItem(9, 0, new QStandardItem(QString("StringIdsSize")));
            model->setItem(9, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->StringIdsSize)));

            model->setItem(10, 0, new QStandardItem(QString("StringIdsOffset")));
            model->setItem(10, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->StringIdsOff)));

            model->setItem(11, 0, new QStandardItem(QString("TypeIdsSize")));
            model->setItem(11, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->TypeIdsSize)));

            model->setItem(12, 0, new QStandardItem(QString("TypeIdsOffset")));
            model->setItem(12, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->TypeIdsOff)));

            model->setItem(13, 0, new QStandardItem(QString("ProtoIdsSize")));
            model->setItem(13, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->ProtoIdsSize)));

            model->setItem(14, 0, new QStandardItem(QString("ProtoIdsOffset")));
            model->setItem(14, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->ProtoIdsOff)));

            model->setItem(15, 0, new QStandardItem(QString("FieldIdsSize")));
            model->setItem(15, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->FieldIdsSize)));

            model->setItem(16, 0, new QStandardItem(QString("FieldIdsOffset")));
            model->setItem(16, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->FieldIdsOff)));

            model->setItem(17, 0, new QStandardItem(QString("MethodIdsSize")));
            model->setItem(17, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->MethodIdsSize)));

            model->setItem(18, 0, new QStandardItem(QString("MethodIdsOffset")));
            model->setItem(18, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->MethodIdsOff)));

            model->setItem(19, 0, new QStandardItem(QString("ClassDefsSize")));
            model->setItem(19, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->ClassDefsSize)));

            model->setItem(20, 0, new QStandardItem(QString("ClassDefsOffset")));
            model->setItem(20, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->ClassDefsOff)));

            model->setItem(21, 0, new QStandardItem(QString("DataSize")));
            model->setItem(21, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->DataSize)));

            model->setItem(22, 0, new QStandardItem(QString("DataOffset")));
            model->setItem(22, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexHeader->DataOff)));


            headerTab->setModel(model);
            headerTab->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        }

        ui->tabWidget->insertTab(0, headerTab, QString("Dex Header"));
        ui->tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::on_actionTypes_Table_triggered()
{
    int index = tabOpened(QString("Types Table"));
    if (index != -1)
        ui->tabWidget->setCurrentIndex(index);
    else
    {
        if (!typesTab)
        {
            typesTab = new QTableView(this);
            typesTab->setWordWrap(true);
            typesTab->setSelectionMode(QAbstractItemView::SingleSelection);
            typesTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
            typesTab->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
            typesTab->verticalHeader()->setDefaultSectionSize(22);
            typesTab->setSelectionBehavior(QAbstractItemView::SelectRows);

            QStandardItemModel *model = new QStandardItemModel(dexFile->nTypeIDs, 2, this);
            model->setHorizontalHeaderItem(0, new QStandardItem(QString("Name")));
            model->setHorizontalHeaderItem(1, new QStandardItem(QString("Description")));

            for (unsigned int i=0; i<dexFile->nTypeIDs; i++)
            {
                model->setItem(i, 0, new QStandardItem(QString((char*)dexFile->StringItems[dexFile->DexTypeIds[i].StringIndex].Data)));
                model->setItem(i, 1, new QStandardItem(QString(
                                                           cDexString::GetTypeDescription((char*)dexFile->StringItems[dexFile->DexTypeIds[i].StringIndex].Data))));
                model->setVerticalHeaderItem(i, new QStandardItem(QString().sprintf("0x%04x", i)));
            }

            typesTab->setModel(model);
            typesTab->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        }

        ui->tabWidget->insertTab(0, typesTab, QString("Types Table"));
        ui->tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::on_actionPrototypes_Table_triggered()
{
    int index = tabOpened(QString("Prototypes Table"));
    if (index != -1)
        ui->tabWidget->setCurrentIndex(index);
    else
    {
        if (!protoTab)
        {
            protoTab = new QTableView(this);
            protoTab->setWordWrap(true);
            protoTab->setSelectionMode(QAbstractItemView::SingleSelection);
            protoTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
            protoTab->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
            protoTab->verticalHeader()->setDefaultSectionSize(22);
            protoTab->setSelectionBehavior(QAbstractItemView::SelectRows);

            QObject::connect(protoTab, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(with_protoTab_doubleClicked(QModelIndex)));

            QStandardItemModel *model = new QStandardItemModel(dexFile->nPrototypeIDs, 3, this);
            model->setHorizontalHeaderItem(0, new QStandardItem(QString("Shorty Index")));
            model->setHorizontalHeaderItem(1, new QStandardItem(QString("ReturnType Index")));
            model->setHorizontalHeaderItem(2, new QStandardItem(QString("Parameters Offset")));

            for (unsigned int i=0; i<dexFile->nPrototypeIDs; i++)
            {
                model->setItem(i, 0, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexProtoIds[i].StringIndex)));
                model->setItem(i, 1, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexProtoIds[i].ReturnTypeIdx)));
                model->setItem(i, 2, new QStandardItem(QString().sprintf("0x%08x", dexFile->DexProtoIds[i].ParametersOff)));
                model->setVerticalHeaderItem(i, new QStandardItem(QString().sprintf("0x%04x", i)));
            }

            protoTab->setModel(model);
            protoTab->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        }

        ui->tabWidget->insertTab(0, protoTab, QString("Prototypes Table"));
        ui->tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::with_protoTab_doubleClicked(const QModelIndex &index)
{
    switch(index.column())
    {
    case 0:
        on_actionStrings_Table_triggered();
        stringsTab->selectRow(dexFile->DexProtoIds[index.row()].StringIndex);
        break;

    case 1:
        on_actionTypes_Table_triggered();
        typesTab->selectRow(dexFile->DexProtoIds[index.row()].ReturnTypeIdx);
        break;

    case 2:
        //on_actionStrings_Table_triggered();
        //stringsTab->selectRow(dexFile->DexFieldIds[index.row()].StringIndex);
        break;
    }
}
void MainWindow::with_methodsTab_doubleClicked(const QModelIndex &index)
{
    switch(index.column())
    {
    case 0:
        on_actionTypes_Table_triggered();
        typesTab->selectRow(dexFile->DexMethodIds[index.row()].ClassIndex);
        break;

    case 1:
        on_actionPrototypes_Table_triggered();
        protoTab->selectRow(dexFile->DexMethodIds[index.row()].PrototypeIndex);
        break;

    case 2:
        on_actionStrings_Table_triggered();
        stringsTab->selectRow(dexFile->DexMethodIds[index.row()].StringIndex);
        break;
    }
}
void MainWindow::on_actionMethods_Table_triggered()
{
    int index = tabOpened(QString("Methods Table"));
    if (index != -1)
        ui->tabWidget->setCurrentIndex(index);
    else
    {
        if (!methodsTab)
        {
            methodsTab = new QTableView(this);
            methodsTab->setWordWrap(true);
            methodsTab->setSelectionMode(QAbstractItemView::SingleSelection);
            methodsTab->setEditTriggers(QAbstractItemView::NoEditTriggers);
            methodsTab->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
            methodsTab->verticalHeader()->setDefaultSectionSize(22);
            methodsTab->setSelectionBehavior(QAbstractItemView::SelectRows);

            QObject::connect(methodsTab, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(with_methodsTab_doubleClicked(QModelIndex)));

            QStandardItemModel *model = new QStandardItemModel(dexFile->nPrototypeIDs, 3, this);
            model->setHorizontalHeaderItem(0, new QStandardItem(QString("Class Index")));
            model->setHorizontalHeaderItem(1, new QStandardItem(QString("Prototype Index")));
            model->setHorizontalHeaderItem(2, new QStandardItem(QString("Name")));

            for (unsigned int i=0; i<dexFile->nPrototypeIDs; i++)
            {
                model->setItem(i, 0, new QStandardItem(QString().sprintf("0x%04x", dexFile->DexMethodIds[i].ClassIndex)));
                model->setItem(i, 1, new QStandardItem(QString().sprintf("0x%04x", dexFile->DexMethodIds[i].PrototypeIndex)));
                model->setItem(i, 2, new QStandardItem(QString((char*)dexFile->StringItems[dexFile->DexMethodIds[i].StringIndex].Data)));
                model->setVerticalHeaderItem(i, new QStandardItem(QString().sprintf("0x%04x", i)));
            }

            methodsTab->setModel(model);
            methodsTab->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        }

        ui->tabWidget->insertTab(0, methodsTab, QString("Methods Table"));
        ui->tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::with_treeView_collapsed(const QModelIndex &index)
{
    ui->treeView->resizeColumnToContents(index.row());
}

void MainWindow::with_treeView_expanded(const QModelIndex &index)
{
    ui->treeView->resizeColumnToContents(index.row());
}


void MainWindow::on_tabWidget_customContextMenuRequested(const QPoint &pos)
{
    std::cout << "context menu" << std::endl;
}

void MainWindow::on_treeView_doubleClicked(const QModelIndex &index)
{
    TreeItem* item = ((TreeModel*)(ui->treeView->model()))->getChild(index);
    if (item->getType() == TI_CLASS)
    {
        addJavaTab(item);
    }
}

void MainWindow::addDexTab(TreeItem* item)
{
    int index = tabOpened(QString::fromStdString(item->getClass()->SourceFile).split('.')[0].append(".dex"));
    if (index >= 0 &&
            ui->tabWidget->tabBar()->tabToolTip(index) ==
            QString::fromStdString(item->getClass()->Package).append('.').append(QString::fromStdString(item->getClass()->Name)))
    {
        ui->tabWidget->setCurrentIndex(index);
        return;
    }

    if (!item->getDexTabWidget())
    {
        CodeEditor* editor = new CodeEditor(this);
        editor->setReadOnly(true);
        printClassDexData(editor, item->getClass());

        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        editor->setTextCursor(cursor);

        item->setDexTabWidget(editor);
    }

    ui->tabWidget->insertTab(ui->tabWidget->count(), item->getDexTabWidget(), QIcon(":/icons/classf_generate.gif"),
                             QString::fromStdString(item->getClass()->SourceFile).split('.')[0].append(".dex"));
    ui->tabWidget->setTabToolTip(ui->tabWidget->count()-1, QString::fromStdString(item->getClass()->Package)
                                 .append('.').append(QString::fromStdString(item->getClass()->Name)));
    ui->tabWidget->setCurrentIndex(ui->tabWidget->count()-1);
}

void MainWindow::printClassDexData(CodeEditor* editor, DEX_DECOMPILED_CLASS* dexClass)
{
    QString output = QString("<font color=\"grey\"><p>/*</p>");
    output.append(QString().sprintf("<p>&nbsp;*&nbsp;&nbsp;%s disassembled by Dexpire</p>", dexClass->SourceFile))
            .append("<p>&nbsp;*/</p></font><p>&nbsp;</p>");

    output.append("<p><b><font color=\"blue\">.class begin</font></b></p><p>&nbsp;</p>");

    output.append("<p><b><font color=\"gray\">.descriptor&nbsp;&nbsp;</font></b>")
            .append(QString::fromStdString(dexClass->Package)).append('.').append(QString::fromStdString(dexClass->Name))
            .append("</p>");

    output.append("<p><b><font color=\"gray\">.superclass&nbsp;&nbsp;</font></b>")
            .append(cDexString::GetTypeDescription((char*)dexClass->Ref->SuperClass))
            .append("</p>");

    output.append("<p><b><font color=\"gray\">.accessflags&nbsp;</font></b>")
            .append(QString::fromStdString(dexClass->AccessFlags))
            .append("</p>");

    output.append("<p><b><font color=\"gray\">.sourcefile&nbsp;&nbsp;</font></b>")
            .append(QString::fromStdString(dexClass->SourceFile))
            .append("</p><p>&nbsp;</p>");

    for (UINT j=0; j<dexClass->Ref->Interfaces.size(); j++)
    {
        if (!j)
            output.append("<p><b><font color=\"blue\">.interfaces begin</font></b></p>");

        output.append("&nbsp;&nbsp;&nbsp;&nbsp;").append((char*)dexClass->Ref->Interfaces[j].c_str());

        if (j+1 == dexClass->Ref->Interfaces.size())
            output.append("<p><b><font color=\"blue\">.interfaces end</font></b></p><p>&nbsp;</p>");
    }

    for (UINT j=0; j<(dexClass->Ref->Ref->ClassDataOff?dexClass->Ref->ClassData->StaticFieldsSize:0); j++)
    {
        if (!j)
            output.append("<p><b><font color=\"blue\">.static-fields begin</font></b></p><p>&nbsp;</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.descriptor&nbsp;&nbsp;</font></b>")
                .append((char*)dexClass->Ref->ClassData->StaticFields[j]->Name)
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.type&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</font></b>")
                .append(cDexString::GetTypeDescription((char*)dexClass->Ref->ClassData->StaticFields[j]->Type))
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.accessflags&nbsp;</font></b>")
                .append(QString(cDexString::GetAccessMask(2, dexClass->Ref->ClassData->StaticFields[j]->AccessFlags)).toLower())
                .append("</p><p>&nbsp;</p>");

        if (j+1 == (dexClass->Ref->Ref->ClassDataOff?dexClass->Ref->ClassData->StaticFieldsSize:0))
            output.append("<p><b><font color=\"blue\">.static-fields end</font></b></p><p>&nbsp;</p>");
    }

    for (UINT j=0; j<(dexClass->Ref->Ref->ClassDataOff?dexClass->Ref->ClassData->InstanceFieldsSize:0); j++)
    {
        if (!j)
            output.append("<p><b><font color=\"blue\">.instance-fields begin</font></b></p><p>&nbsp;</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.descriptor&nbsp;&nbsp;</font></b>")
                .append((char*)dexClass->Ref->ClassData->InstanceFields[j]->Name)
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.type&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</font></b>")
                .append(cDexString::GetTypeDescription((char*)dexClass->Ref->ClassData->InstanceFields[j]->Type))
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.accessflags&nbsp;</font></b>")
                .append(QString(cDexString::GetAccessMask(2, dexClass->Ref->ClassData->InstanceFields[j]->AccessFlags)).toLower())
                .append("</p><p>&nbsp;</p>");

        if (j+1 == (dexClass->Ref->Ref->ClassDataOff?dexClass->Ref->ClassData->InstanceFieldsSize:0))
            output.append("<p><b><font color=\"blue\">.instance-fields end</font></b></p><p>&nbsp;</p>");
    }

    for (UINT j=0; j<(dexClass->Ref->Ref->ClassDataOff?dexClass->Ref->ClassData->DirectMethodsSize:0); j++)
    {
        output.append("<p><b><font color=\"blue\">.direct-method begin</font></b></p><p>&nbsp;</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.name&nbsp;&nbsp;&nbsp;</font></b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")
                .append(QString((char*)dexClass->Ref->ClassData->DirectMethods[j]->Name).replace('<', "&#60;").replace('>', "&#62;"))
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.type&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</font></b>")
                .append(cDexString::GetTypeDescription((char*)dexClass->Ref->ClassData->DirectMethods[j]->ProtoType))
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.accessflags&nbsp;&nbsp;</font></b>")
                .append(QString(cDexString::GetAccessMask(1, dexClass->Ref->ClassData->DirectMethods[j]->AccessFlags)).toLower())
                .append("</p>");

        if (dexClass->Ref->ClassData->DirectMethods[j]->CodeArea)
        {
            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.registers&nbsp;</font></b>&nbsp;&nbsp;&nbsp;")
                    .append(QString::number(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->RegistersSize))
                    .append("</p>");

            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.ins&nbsp;</font></b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")
                    .append(QString::number(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->InsSize))
                    .append("</p>");

            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.outs&nbsp;</font></b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")
                    .append(QString::number(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->OutsSize))
                    .append("</p>");

            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.instructions&nbsp;</font></b>")
                    .append(QString::number(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->InstBufferSize))
                    .append("</p>");

            UINT line = 0;
            QString code_line;
            for (UINT k=0; k<dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions.size(); k++)
            {
                code_line = QString().sprintf("<p>%06x:&nbsp;", dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->Offset & 0x00FFFFFF);
                for (UINT l=0; l<(UINT)(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->BufferSize>5?5:
                                        dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->BufferSize); l++)
                    code_line.append(QString().sprintf("%04x&nbsp;", SWAP_SHORT(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->Buffer[l])));

                for (UINT l=0; l<28 - (UINT)(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->BufferSize>5?5:
                                             dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->BufferSize)*5; l++)
                    code_line.append("&nbsp;");

                code_line.append(QString().sprintf("|%04x:&nbsp;%s</p>", line,
                                                   dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->Decoded));
                output.append(code_line);
                line += dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Instructions[k]->BufferSize/2;
            }

            for (UINT k=0; k<dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->TriesSize; k++)
            {
                if (!k)
                    output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.catches&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</font></b>")
                            .append(QString::number(dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->TriesSize))
                            .append("</p>");

                output.append(QString().sprintf("<p>0x%04x - 0x%04x",
                                                dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Tries[k]->InstructionsStart,
                                                dexClass->Ref->ClassData->DirectMethods[j]->CodeArea->Tries[k]->InstructionsEnd))
                        .append("</p>");

                //for (UINT l=0; l<(UINT)dexClass->Ref->ClassData->DirectMethods[j].CodeArea->Tries[k].CatchHandler->TypeHandlersSize; l++)
                //{
                //    editor->appendPlainText(QString.sprintf("          %s -> 0x%04x\n",
                //        dexClass->Ref->ClassData->DirectMethods[j].CodeArea->Tries[k].CatchHandler->TypeHandlers[l].Type,
                //        dexClass->Ref->ClassData->DirectMethods[j].CodeArea->Tries[k].CatchHandler->TypeHandlers[l].Address);
                //}
            }
        }

        output.append("<p>&nbsp;</p>");
        output.append("<p><b><font color=\"blue\">.direct-method end</font></b></p><p>&nbsp;</p>");
    }

    for (UINT j=0; j<(dexClass->Ref->Ref->ClassDataOff?dexClass->Ref->ClassData->VirtualMethodsSize:0); j++)
    {
        output.append("<p><b><font color=\"blue\">.virtual-method begin</font></b></p><p>&nbsp;</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.name&nbsp;&nbsp;&nbsp;</font></b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")
                .append(QString((char*)dexClass->Ref->ClassData->VirtualMethods[j]->Name).replace('<', "&#60;").replace('>', "&#62;"))
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.type&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</font></b>")
                .append(cDexString::GetTypeDescription((char*)dexClass->Ref->ClassData->VirtualMethods[j]->ProtoType))
                .append("</p>");

        output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.accessflags&nbsp;&nbsp;</font></b>")
                .append(QString(cDexString::GetAccessMask(1, dexClass->Ref->ClassData->VirtualMethods[j]->AccessFlags)).toLower())
                .append("</p>");

        if (dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea)
        {
            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.registers&nbsp;</font></b>&nbsp;&nbsp;&nbsp;")
                    .append(QString::number(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->RegistersSize))
                    .append("</p>");

            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.ins&nbsp;</font></b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")
                    .append(QString::number(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->InsSize))
                    .append("</p>");

            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.outs&nbsp;</font></b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;")
                    .append(QString::number(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->OutsSize))
                    .append("</p>");

            output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.instructions&nbsp;</font></b>")
                    .append(QString::number(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->InstBufferSize))
                    .append("</p>");

            UINT line = 0;
            QString code_line;
            for (UINT k=0; k<dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions.size(); k++)
            {
                code_line = QString().sprintf("<p>%06x:&nbsp;", dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->Offset & 0x00FFFFFF);
                for (UINT l=0; l<(UINT)(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->BufferSize>5?5:
                                        dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->BufferSize); l++)
                    code_line.append(QString().sprintf("%04x&nbsp;", SWAP_SHORT(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->Buffer[l])));

                for (UINT l=0; l<28 - (UINT)(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->BufferSize>5?5:
                                             dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->BufferSize)*5; l++)
                    code_line.append("&nbsp;");

                code_line.append(QString().sprintf("|%04x:&nbsp;%s</p>", line,
                                                   dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->Decoded));
                output.append(code_line);
                line += dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Instructions[k]->BufferSize/2;
            }

            for (UINT k=0; k<dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->TriesSize; k++)
            {
                if (!k)
                    output.append("<p><b><font color=\"gray\">&nbsp;&nbsp;&nbsp;&nbsp;.catches&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</font></b>")
                            .append(QString::number(dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->TriesSize))
                            .append("</p>");

                output.append(QString().sprintf("<p>0x%04x - 0x%04x",
                                                dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Tries[k]->InstructionsStart,
                                                dexClass->Ref->ClassData->VirtualMethods[j]->CodeArea->Tries[k]->InstructionsEnd))
                        .append("</p>");

                //for (UINT l=0; l<(UINT)dexClass->Ref->ClassData->VirtualMethods[j].CodeArea->Tries[k]->CatchHandler->TypeHandlersSize; l++)
                //{
                //    output.append(QString().sprintf("          %s -> 0x%04x\n",
                //        dexClass->Ref->ClassData->VirtualMethods[j].CodeArea->Tries[k]->CatchHandler->TypeHandlers[l].Type,
                //        dexClass->Ref->ClassData->VirtualMethods[j].CodeArea->Tries[k]->CatchHandler->TypeHandlers[l].Address));
                //}
            }
        }

        output.append("<p>&nbsp;</p>");
        output.append("<p><b><font color=\"blue\">.virtual-method end</font></b></p><p>&nbsp;</p>");
    }

    output.append("<b><font color=\"blue\">.class end</font></b>");
    editor->appendHtml(output);
}

void MainWindow::addJavaTab(TreeItem* item)
{
    int index = tabOpened(QString::fromStdString(item->getClass()->SourceFile));
    if (index >= 0 &&
            ui->tabWidget->tabBar()->tabToolTip(index) ==
            QString::fromStdString(item->getClass()->Package)
            .append('.').append(QString::fromStdString(item->getClass()->Name)))
    {
        ui->tabWidget->setCurrentIndex(index);
        return;
    }

    if (!item->getJavaTabWidget())
    {
        CodeEditor* editor = new CodeEditor(this);
        editor->setReadOnly(true);
        printClassJavaData(editor, item->getClass());

        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        editor->setTextCursor(cursor);

        item->setJavaTabWidget(editor);
    }

    ui->tabWidget->insertTab(ui->tabWidget->count(), item->getJavaTabWidget(), QIcon(":/icons/java.gif"), QString::fromStdString(item->getClass()->SourceFile));
    ui->tabWidget->setTabToolTip(ui->tabWidget->count()-1, QString::fromStdString(item->getClass()->Package).append('.').append(QString::fromStdString(item->getClass()->Name)));
    ui->tabWidget->setCurrentIndex(ui->tabWidget->count()-1);
}

void MainWindow::printClassBodyJava(QString& output, struct DEX_DECOMPILED_CLASS* dexClass, int depth)
{
    //if (re->exactMatch(dexClass->Name))
    //    return;

    output.append("<p>");
    for(int i=0; i<depth; i++) output.append("&nbsp;&nbsp;&nbsp;&nbsp;");

    if (QString::fromStdString(dexClass->AccessFlags).contains("interface"))
    {
        output.append("<b><font color=\"blue\">")
                .append(QString::fromStdString(dexClass->AccessFlags).append("</b></font> ").append(QString::fromStdString(dexClass->Name)).trimmed());

        if (dexClass->Interfaces.size())
            output.append("<b><font color=\"blue\"> extends</b></font>");

        for (UINT j=0; j<dexClass->Interfaces.size(); j++)
        {
            if (j)  output.append(",");
            output.append(QString().sprintf(" %s", cDexString::ExtractShortLType((char*)dexClass->Interfaces[j].c_str())));
        }
    }
    else
    {
        output.append("<b><font color=\"blue\">")
                .append(QString::fromStdString(dexClass->AccessFlags).append(" class</b></font> ")
                        .append(QString::fromStdString(dexClass->Name)).trimmed());

        if (dexClass->Extends.size())
            output.append("<b><font color=\"blue\"> extends</b></font>");

        for (UINT j=0; j<dexClass->Extends.size(); j++)
        {
            if (j)  output.append(",");
            output.append(QString().sprintf(" %s", cDexString::ExtractShortLType((char*)dexClass->Extends[j].c_str())));
        }

        if (dexClass->Interfaces.size())
            output.append("<b><font color=\"blue\"> implements</b></font>");

        for (UINT j=0; j<dexClass->Interfaces.size(); j++)
        {
            if (j)  output.append(",");
            output.append(QString().sprintf(" %s", cDexString::ExtractShortLType((char*)dexClass->Interfaces[j].c_str())));
        }
    }

    output.append(" {</p><p>&nbsp;</p>");

    for (UINT j=0; j<dexClass->Fields.size(); j++)
    {
        if (dexClass->Fields[j]->Ref->AccessFlags & ACC_SYNTHETIC)
            continue;

        output.append("<p>");
        for(int i=0; i<depth; i++) output.append("&nbsp;&nbsp;&nbsp;&nbsp;");
        output.append("&nbsp;&nbsp;&nbsp;&nbsp;");

        if (dexClass->Fields[j]->AccessFlags.size())
            output.append("<b><font color=\"blue\">")
                .append(QString::fromStdString(dexClass->Fields[j]->AccessFlags))
                .append("</font></b>&nbsp;");

        output.append(cDexString::ExtractShortLType((char*)dexClass->Fields[j]->ReturnType.c_str()))
                .append("&nbsp;").append(QString::fromStdString(dexClass->Fields[j]->Name));

        if (dexClass->Fields[j]->Value.size())
            output.append(" = ").append(QString::fromStdString(dexClass->Fields[j]->Value));

        output.append(";</p>");
        if (j+1 == dexClass->Fields.size())
            output.append("<p>&nbsp;</p>");
    }


    for (UINT j=0; j<dexClass->Methods.size(); j++)
    {
        if (dexClass->Methods[j]->Name ==  "<init>" ||dexClass->Methods[j]->Name == "<clinit>")
            continue;

        output.append("<p>");
        for(int i=0; i<depth; i++) output.append("&nbsp;&nbsp;&nbsp;&nbsp;");
        output.append("&nbsp;&nbsp;&nbsp;&nbsp;<b><font color=\"blue\">")
                .append(QString::fromStdString(dexClass->Methods[j]->AccessFlags))
                .append("</font></b>").append(dexClass->Methods[j]->AccessFlags.size()? "&nbsp;":"")
                .append(cDexString::ExtractShortLType((char*)dexClass->Methods[j]->ReturnType.c_str()))
                .append("&nbsp;").append(QString::fromStdString(dexClass->Methods[j]->Name)).append("(");

        for (UINT k=0; k<dexClass->Methods[j]->Arguments.size(); k++)
        {
            if (k) output.append(", ");
            output.append(QString().sprintf("%s %s",
                    (char*)dexClass->Methods[j]->Arguments[k]->ShortType.c_str(),
                    (char*)dexClass->Methods[j]->Arguments[k]->Name.c_str()));
        }

        output.append(") {</p>");

        for(unsigned int k=0; k<dexClass->Methods[j]->Decompiled.size(); k++)
        {
            output.append("<p>");
            for(int i=0; i<depth; i++) output.append("&nbsp;&nbsp;&nbsp;&nbsp;");
            output.append(QString().sprintf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%s;</p>",
                   dexClass->Methods[j]->Decompiled[k].c_str()));
        }

        /*for (UINT k=0; k<dexClass->Methods[j]->LinesSize; k++)
        {
            if (dexClass->Methods[j]->Lines[k]->Decompiled)
            {
                output.append("<p>");
                for(int i=0; i<depth; i++) output.append("&nbsp;&nbsp;&nbsp;&nbsp;");
                output.append(QString().sprintf("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%s;</p>",
                       dexClass->Methods[j]->Lines[k]->Decompiled));
            }
            /*
            else
            {
                for (UINT l=0; l<dexClass->Methods[j]->Lines[k]->InstructionsSize; l++)
                {
                    if (k && !l) output.append("<p>&nbsp;</p>");
                    output.append(QString().sprintf("<p>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%s</p>",
                                dexClass->Methods[j]->Lines[k]->Instructions[l]->Decoded));
                }
            }
            */
        //}

        output.append("<p>");
        for(int i=0; i<depth; i++) output.append("&nbsp;&nbsp;&nbsp;&nbsp;");
        output.append("&nbsp;&nbsp;&nbsp;&nbsp;}</p><p>&nbsp;</p>");
    }

    for (unsigned int i=0; i<dexClass->SubClasses.size(); i++)
        printClassBodyJava(output, dexClass->SubClasses[i], depth+1);

    output.append("<p>");
    for(int i=0; i<depth; i++) output.append("&nbsp;&nbsp;&nbsp;&nbsp;");
    output.append("}</p><p>&nbsp;</p>");
}

void MainWindow::printClassJavaData(CodeEditor* editor, struct DEX_DECOMPILED_CLASS* dexClass)
{
    QString output = QString("<font color=\"grey\"><p>/*</p>");
    output.append(QString().sprintf("<p>&nbsp;*&nbsp;&nbsp;%s decompiled by Dexpire</p>", dexClass->SourceFile))
            .append("<p>&nbsp;*/</p></font><p>&nbsp;</p>");

    output.append(QString().sprintf("<p><b><font color=\"blue\">package</font></b>&nbsp;%s;</p><p>&nbsp;</p>", dexClass->Package.c_str()));

    for (UINT j=0; j<dexClass->Imports.size(); j++)
    {
        output.append(QString().sprintf("<p><b><font color=\"blue\">import</font></b>&nbsp;%s;</p>", dexClass->Imports[j].c_str()));
        if (j+1 == dexClass->Imports.size())
            output.append("<p>&nbsp;</p>");
    }

    printClassBodyJava(output, dexClass, 0);
    editor->appendHtml(output);
}

void MainWindow::setApkToolbarEnabled(bool enable)
{
    ui->menuAPK_Structure->setEnabled(enable);

    ui->actionAndroid_Manifest->setEnabled(enable);
    ui->actionAndroid_Manifest_2->setEnabled(enable);

    ui->actionCertificates->setEnabled(enable);
    ui->actionCertificates_2->setEnabled(enable);

    ui->actionResources->setEnabled(enable);
    ui->actionResources_2->setEnabled(enable);
}

void MainWindow::setDexToolbarEnabled(bool enable)
{
    ui->menuDex_Structure->setEnabled(enable);

    ui->actionSave_All->setEnabled(enable);
    ui->actionSave_All_2->setEnabled(enable);

    ui->actionHeader->setEnabled(enable);
    ui->actionHeader_2->setEnabled(enable);

    ui->actionFields_Table->setEnabled(enable);
    ui->actionFields_Table_2->setEnabled(enable);

    ui->actionMethods_Table->setEnabled(enable);
    ui->actionMethods_Table_2->setEnabled(enable);

    ui->actionPrototypes_Table->setEnabled(enable);
    ui->actionPrototypes_Table_2->setEnabled(enable);

    ui->actionStrings_Table->setEnabled(enable);
    ui->actionStrings_Table_2->setEnabled(enable);

    ui->actionTypes_Table->setEnabled(enable);
    ui->actionTypes_Table_2->setEnabled(enable);
}

void MainWindow::setClassToolbarEnabled(bool enable)
{
    ui->actionSave_Class->setEnabled(enable);
    ui->actionDex_Disassembly->setEnabled(enable);
    ui->actionJava_Source->setEnabled(enable);

    ui->actionSave_Class_2->setEnabled(enable);
    ui->actionDex_Disassembly_2->setEnabled(enable);
    ui->actionJava_Source_2->setEnabled(enable);
}

void MainWindow::on_actionJava_Source_triggered()
{
    TreeItem* item = ((TreeModel*)(ui->treeView->model()))->getChild( ui->treeView->currentIndex());
    if (item->getType() == TI_CLASS)
        addJavaTab(item);
}
