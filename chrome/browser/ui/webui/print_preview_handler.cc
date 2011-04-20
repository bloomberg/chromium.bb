// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_handler.h"

#include <string>

#include "base/i18n/file_util_icu.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_ui_html_source.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/print_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/print_job_constants.h"

#if defined(OS_POSIX) && !defined(OS_CHROMEOS)
#include <cups/cups.h>

#include "base/file_util.h"
#endif

namespace {

const bool kColorDefaultValue = false;
const bool kLandscapeDefaultValue = false;

const char kDisableColorOption[] = "disableColorOption";
const char kSetColorAsDefault[] = "setColorAsDefault";

#if defined(OS_POSIX) && !defined(OS_CHROMEOS)
const char kColorDevice[] = "ColorDevice";
#endif

TabContents* GetInitiatorTab(TabContents* preview_tab) {
  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return NULL;
  return tab_controller->GetInitiatorTab(preview_tab);
}

// Get the print job settings dictionary from |args|. The caller takes
// ownership of the returned DictionaryValue. Returns NULL on failure.
DictionaryValue* GetSettingsDictionary(const ListValue* args) {
  std::string json_str;
  if (!args->GetString(0, &json_str)) {
    NOTREACHED() << "Could not read JSON argument";
    return NULL;
  }
  if (json_str.empty()) {
    NOTREACHED() << "Empty print job settings";
    return NULL;
  }
  scoped_ptr<DictionaryValue> settings(static_cast<DictionaryValue*>(
      base::JSONReader::Read(json_str, false)));
  if (!settings.get() || !settings->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED() << "Print job settings must be a dictionary.";
    return NULL;
  }

  if (settings->empty()) {
    NOTREACHED() << "Print job settings dictionary is empty";
    return NULL;
  }

  return settings.release();
}

}  // namespace

class PrintSystemTaskProxy
    : public base::RefCountedThreadSafe<PrintSystemTaskProxy,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  PrintSystemTaskProxy(const base::WeakPtr<PrintPreviewHandler>& handler,
                             printing::PrintBackend* print_backend)
      : handler_(handler),
        print_backend_(print_backend) {
  }

  void EnumeratePrinters() {
    ListValue* printers = new ListValue;
    int default_printer_index = -1;

    printing::PrinterList printer_list;
    print_backend_->EnumeratePrinters(&printer_list);
    int i = 0;
    for (printing::PrinterList::iterator index = printer_list.begin();
         index != printer_list.end(); ++index, ++i) {
      printers->Append(new StringValue(index->printer_name));
      if (index->is_default)
        default_printer_index = i;
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &PrintSystemTaskProxy::SendPrinterList,
                          printers,
                          new FundamentalValue(default_printer_index)));
  }

  void SendPrinterList(ListValue* printers,
                       FundamentalValue* default_printer_index) {
    if (handler_)
      handler_->SendPrinterList(*printers, *default_printer_index);
    delete printers;
    delete default_printer_index;
  }

  void GetPrinterCapabilities(const std::string& printer_name) {
    printing::PrinterCapsAndDefaults printer_info;
    bool supports_color = true;
    if (!print_backend_->GetPrinterCapsAndDefaults(printer_name,
                                                   &printer_info)) {
      return;
    }

  #if defined(OS_POSIX) && !defined(OS_CHROMEOS)
    FilePath ppd_file_path;
    if (!file_util::CreateTemporaryFile(&ppd_file_path))
      return;

    int data_size = printer_info.printer_capabilities.length();
    if (data_size != file_util::WriteFile(
                         ppd_file_path,
                         printer_info.printer_capabilities.data(),
                         data_size)) {
      file_util::Delete(ppd_file_path, false);
      return;
    }

    ppd_file_t* ppd = ppdOpenFile(ppd_file_path.value().c_str());
    if (ppd) {
      ppd_attr_t* attr = ppdFindAttr(ppd, kColorDevice, NULL);
      if (attr && attr->value)
        supports_color = ppd->color_device;
      ppdClose(ppd);
    }
    file_util::Delete(ppd_file_path, false);
  #elif defined(OS_WIN) || defined(OS_CHROMEOS)
    NOTIMPLEMENTED();
  #endif

    DictionaryValue settings_info;
    settings_info.SetBoolean(kDisableColorOption, !supports_color);
    settings_info.SetBoolean(kSetColorAsDefault, false);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &PrintSystemTaskProxy::SendPrinterCapabilities,
                          settings_info.DeepCopy()));
  }

  void SendPrinterCapabilities(DictionaryValue* settings_info) {
    if (handler_)
      handler_->SendPrinterCapabilities(*settings_info);
    delete settings_info;
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<PrintSystemTaskProxy>;

  ~PrintSystemTaskProxy() {}

  base::WeakPtr<PrintPreviewHandler> handler_;

  scoped_refptr<printing::PrintBackend> print_backend_;

  DISALLOW_COPY_AND_ASSIGN(PrintSystemTaskProxy);
};

// A Task implementation that stores a PDF file on disk.
class PrintToPdfTask : public Task {
 public:
  // Takes ownership of |metafile|.
  PrintToPdfTask(printing::Metafile* metafile, const FilePath& path)
      : metafile_(metafile), path_(path) {
  }

  ~PrintToPdfTask() {}

  // Task implementation
  virtual void Run() {
    metafile_->SaveTo(path_);
  }

 private:
  // The metafile holding the PDF data.
  scoped_ptr<printing::Metafile> metafile_;

  // The absolute path where the file will be saved.
  FilePath path_;
};

// static
FilePath* PrintPreviewHandler::last_saved_path_ = NULL;

PrintPreviewHandler::PrintPreviewHandler()
    : print_backend_(printing::PrintBackend::CreateInstance(NULL)) {
}

PrintPreviewHandler::~PrintPreviewHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void PrintPreviewHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getPrinters",
      NewCallback(this, &PrintPreviewHandler::HandleGetPrinters));
  web_ui_->RegisterMessageCallback("getPreview",
      NewCallback(this, &PrintPreviewHandler::HandleGetPreview));
  web_ui_->RegisterMessageCallback("print",
      NewCallback(this, &PrintPreviewHandler::HandlePrint));
  web_ui_->RegisterMessageCallback("getPrinterCapabilities",
      NewCallback(this, &PrintPreviewHandler::HandleGetPrinterCapabilities));
}

void PrintPreviewHandler::HandleGetPrinters(const ListValue*) {
  scoped_refptr<PrintSystemTaskProxy> task =
      new PrintSystemTaskProxy(AsWeakPtr(), print_backend_.get());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &PrintSystemTaskProxy::EnumeratePrinters));
}

void PrintPreviewHandler::HandleGetPreview(const ListValue* args) {
  TabContents* initiator_tab = GetInitiatorTab(web_ui_->tab_contents());
  if (!initiator_tab)
    return;
  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

  RenderViewHost* rvh = initiator_tab->render_view_host();
  rvh->Send(new PrintMsg_PrintPreview(rvh->routing_id(), *settings));
}

void PrintPreviewHandler::HandlePrint(const ListValue* args) {
  TabContents* initiator_tab = GetInitiatorTab(web_ui_->tab_contents());
  if (initiator_tab) {
    RenderViewHost* rvh = initiator_tab->render_view_host();
    rvh->Send(new PrintMsg_ResetScriptedPrintCount(rvh->routing_id()));
  }

  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

  bool print_to_pdf = false;
  settings->GetBoolean(printing::kSettingPrintToPDF, &print_to_pdf);

  if (print_to_pdf) {
    // Pre-populating select file dialog with print job title.
    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(
            web_ui_->tab_contents());

    string16 print_job_title_utf16 =
        wrapper->print_view_manager()->RenderSourceName();

#if defined(OS_WIN)
    FilePath::StringType print_job_title(print_job_title_utf16);
#elif defined(OS_POSIX)
    FilePath::StringType print_job_title = UTF16ToUTF8(print_job_title_utf16);
#endif

    file_util::ReplaceIllegalCharactersInPath(&print_job_title, '_');
    FilePath default_filename(print_job_title);
    default_filename =
        default_filename.ReplaceExtension(FILE_PATH_LITERAL("pdf"));

    SelectFile(default_filename);
  } else {
    RenderViewHost* rvh = web_ui_->GetRenderViewHost();
    rvh->Send(new PrintMsg_PrintForPrintPreview(rvh->routing_id(), *settings));
  }
}

void PrintPreviewHandler::HandleGetPrinterCapabilities(
    const ListValue* args) {
  std::string printer_name;
  bool ret = args->GetString(0, &printer_name);
  if (!ret || printer_name.empty())
    return;

  scoped_refptr<PrintSystemTaskProxy> task =
      new PrintSystemTaskProxy(AsWeakPtr(), print_backend_.get());

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &PrintSystemTaskProxy::GetPrinterCapabilities,
                        printer_name));
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const DictionaryValue& settings_info) {
  web_ui_->CallJavascriptFunction("updateWithPrinterCapabilities",
                                  settings_info);
}

void PrintPreviewHandler::SendPrinterList(
    const ListValue& printers,
    const FundamentalValue& default_printer_index) {
  web_ui_->CallJavascriptFunction("setPrinters", printers,
                                  default_printer_index);
}

void PrintPreviewHandler::SelectFile(const FilePath& default_filename) {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pdf"));

  // Initializing last_saved_path_ if it is not already initialized.
  if (!last_saved_path_) {
    last_saved_path_ = new FilePath();
    // Allowing IO operation temporarily. It is ok to do so here because
    // the select file dialog performs IO anyway in order to display the
    // folders and also it is modal.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, last_saved_path_);
  }

  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_SAVEAS_FILE,
      string16(),
      last_saved_path_->Append(default_filename),
      &file_type_info,
      0,
      FILE_PATH_LITERAL(""),
      web_ui_->tab_contents(),
      platform_util::GetTopLevel(
          web_ui_->tab_contents()->GetNativeView()),
      NULL);
}

void PrintPreviewHandler::FileSelected(const FilePath& path,
                                       int index, void* params) {
  PrintPreviewUIHTMLSource::PrintPreviewData data;
  PrintPreviewUI* pp_ui = reinterpret_cast<PrintPreviewUI*>(web_ui_);
  pp_ui->html_source()->GetPrintPreviewData(&data);
  DCHECK(data.first != NULL);
  DCHECK(data.second > 0);

  printing::PreviewMetafile* metafile = new printing::PreviewMetafile;
  metafile->InitFromData(data.first->memory(), data.second);

  // Updating last_saved_path_ to the newly selected folder.
  *last_saved_path_ = path.DirName();

  PrintToPdfTask* task = new PrintToPdfTask(metafile, path);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, task);
}
