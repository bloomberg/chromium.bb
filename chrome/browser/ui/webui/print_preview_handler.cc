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

namespace {

const bool kColorDefaultValue = false;
const bool kLandscapeDefaultValue = false;

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

class EnumeratePrintersTaskProxy
    : public base::RefCountedThreadSafe<EnumeratePrintersTaskProxy,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  EnumeratePrintersTaskProxy(const base::WeakPtr<PrintPreviewHandler>& handler,
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
                          &EnumeratePrintersTaskProxy::SendPrinterList,
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

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<EnumeratePrintersTaskProxy>;

  ~EnumeratePrintersTaskProxy() {}

  base::WeakPtr<PrintPreviewHandler> handler_;

  scoped_refptr<printing::PrintBackend> print_backend_;

  DISALLOW_COPY_AND_ASSIGN(EnumeratePrintersTaskProxy);
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
    : print_backend_(printing::PrintBackend::CreateInstance(NULL)),
      need_to_generate_preview_(true),
      color_(kColorDefaultValue),
      landscape_(kLandscapeDefaultValue) {
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
}

void PrintPreviewHandler::HandleGetPrinters(const ListValue*) {
  scoped_refptr<EnumeratePrintersTaskProxy> task =
      new EnumeratePrintersTaskProxy(AsWeakPtr(), print_backend_.get());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &EnumeratePrintersTaskProxy::EnumeratePrinters));
}

void PrintPreviewHandler::HandleGetPreview(const ListValue* args) {
  TabContents* initiator_tab = GetInitiatorTab(web_ui_->tab_contents());
  if (!initiator_tab)
    return;
  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

  // Handle settings that do not require print preview regeneration.
  ProcessColorSetting(*settings);

  // Handle settings that require print preview regeneration.
  ProcessLandscapeSetting(*settings);

  if (need_to_generate_preview_) {
    RenderViewHost* rvh = initiator_tab->render_view_host();
    rvh->Send(new PrintMsg_PrintPreview(rvh->routing_id(), *settings));
    need_to_generate_preview_ = false;
  }
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

void PrintPreviewHandler::SendPrinterList(
    const ListValue& printers,
    const FundamentalValue& default_printer_index) {
  web_ui_->CallJavascriptFunction("setPrinters", printers,
                                  default_printer_index);
}

void PrintPreviewHandler::ProcessColorSetting(const DictionaryValue& settings) {
  bool color = kColorDefaultValue;
  settings.GetBoolean(printing::kSettingColor, &color);
  if (color != color_) {
    color_ = color;
    FundamentalValue color_value(color_);
    web_ui_->CallJavascriptFunction("setColor", color_value);
  }
}

void PrintPreviewHandler::ProcessLandscapeSetting(
    const DictionaryValue& settings) {
  bool landscape = kLandscapeDefaultValue;
  settings.GetBoolean(printing::kSettingLandscape, &landscape);
  if (landscape != landscape_) {
    landscape_ = landscape;
    need_to_generate_preview_ = true;
  }
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
