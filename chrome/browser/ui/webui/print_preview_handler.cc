// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_handler.h"

#include <string>

#include "base/base64.h"
#if !defined(OS_CHROMEOS)
#include "base/command_line.h"
#endif
#include "base/i18n/file_util_icu.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/cloud_print_signin_dialog.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_paths.h"
#if !defined(OS_CHROMEOS)
#include "chrome/common/chrome_switches.h"
#endif
#include "chrome/common/print_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/page_range.h"
#include "printing/print_job_constants.h"

#if defined(USE_CUPS)
#include <cups/cups.h>
#include <cups/ppd.h>

#include "base/file_util.h"
#endif

namespace {

const char kDisableColorOption[] = "disableColorOption";
const char kSetColorAsDefault[] = "setColorAsDefault";
const char kSetDuplexAsDefault[] = "setDuplexAsDefault";

#if defined(USE_CUPS)
const char kColorDevice[] = "ColorDevice";
const char kDuplex[] = "Duplex";
const char kDuplexNone[] = "None";
#elif defined(OS_WIN)
const char kPskColor[] = "psk:Color";
const char kPskDuplexFeature[] = "psk:JobDuplexAllDocumentsContiguously";
const char kPskTwoSided[] = "psk:TwoSided";
#endif

enum UserActionBuckets {
  PRINT_TO_PRINTER,
  PRINT_TO_PDF,
  CANCEL,
  FALLBACK_TO_ADVANCED_SETTINGS_DIALOG,
  PREVIEW_FAILED,
  PREVIEW_STARTED,
  INITIATOR_TAB_CRASHED,
  INITIATOR_TAB_CLOSED,
  USERACTION_BUCKET_BOUNDARY
};

enum PrintSettingsBuckets {
  LANDSCAPE,
  PORTRAIT,
  COLOR,
  BLACK_AND_WHITE,
  COLLATE,
  SIMPLEX,
  DUPLEX,
  PRINT_SETTINGS_BUCKET_BOUNDARY
};

void ReportUserActionHistogram(enum UserActionBuckets event) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.UserAction", event,
                            USERACTION_BUCKET_BOUNDARY);
}

void ReportPrintSettingHistogram(enum PrintSettingsBuckets setting) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.PrintSettings", setting,
                            PRINT_SETTINGS_BUCKET_BOUNDARY);
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

int GetPageCountFromSettingsDictionary(const DictionaryValue& settings) {
  int count = 0;
  ListValue* page_range_array;
  if (settings.GetList(printing::kSettingPageRange, &page_range_array)) {
    for (size_t index = 0; index < page_range_array->GetSize(); ++index) {
      DictionaryValue* dict;
      if (!page_range_array->GetDictionary(index, &dict))
        continue;

      printing::PageRange range;
      if (!dict->GetInteger(printing::kSettingPageRangeFrom, &range.from) ||
          !dict->GetInteger(printing::kSettingPageRangeTo, &range.to)) {
        continue;
      }
      count += (range.to - range.from) + 1;
    }
  }
  return count;
}

// Track the popularity of print settings and report the stats.
void ReportPrintSettingsStats(const DictionaryValue& settings) {
  bool landscape;
  if (settings.GetBoolean(printing::kSettingLandscape, &landscape))
    ReportPrintSettingHistogram(landscape ? LANDSCAPE : PORTRAIT);

  bool collate;
  if (settings.GetBoolean(printing::kSettingCollate, &collate) && collate)
    ReportPrintSettingHistogram(COLLATE);

  int duplex_mode;
  if (settings.GetInteger(printing::kSettingDuplexMode, &duplex_mode))
    ReportPrintSettingHistogram(duplex_mode ? DUPLEX : SIMPLEX);

  bool is_color;
  if (settings.GetBoolean(printing::kSettingColor, &is_color))
    ReportPrintSettingHistogram(is_color ? COLOR : BLACK_AND_WHITE);
}

printing::BackgroundPrintingManager* GetBackgroundPrintingManager() {
  return g_browser_process->background_printing_manager();
}

}  // namespace

class PrintSystemTaskProxy
    : public base::RefCountedThreadSafe<PrintSystemTaskProxy,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  PrintSystemTaskProxy(const base::WeakPtr<PrintPreviewHandler>& handler,
                       printing::PrintBackend* print_backend,
                       bool has_logged_printers_count)
      : handler_(handler),
        print_backend_(print_backend),
        has_logged_printers_count_(has_logged_printers_count) {
  }

  void GetDefaultPrinter() {
    VLOG(1) << "Get default printer start";
    StringValue* default_printer = NULL;
    if (PrintPreviewHandler::last_used_printer_name_ == NULL) {
      default_printer = new StringValue(
          print_backend_->GetDefaultPrinterName());
    } else {
      default_printer = new StringValue(
          *PrintPreviewHandler::last_used_printer_name_);
    }
    std::string default_printer_string;
    default_printer->GetAsString(&default_printer_string);
    VLOG(1) << "Get default printer finished, found: "
            << default_printer_string;

    StringValue* cloud_print_data = NULL;
    if (PrintPreviewHandler::last_used_printer_cloud_print_data_ != NULL) {
      cloud_print_data = new StringValue(
          *PrintPreviewHandler::last_used_printer_cloud_print_data_);
    } else {
      cloud_print_data = new StringValue("");
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &PrintSystemTaskProxy::SendDefaultPrinter,
                          default_printer,
                          cloud_print_data));
  }

  void SendDefaultPrinter(const StringValue* default_printer,
                          const StringValue* cloud_print_data) {
    if (handler_)
      handler_->SendDefaultPrinter(*default_printer, *cloud_print_data);
    delete default_printer;
  }

  void EnumeratePrinters() {
    VLOG(1) << "Enumerate printers start";
    ListValue* printers = new ListValue;

    printing::PrinterList printer_list;
    print_backend_->EnumeratePrinters(&printer_list);

    if (!has_logged_printers_count_) {
      // Record the total number of printers.
      UMA_HISTOGRAM_COUNTS("PrintPreview.NumberOfPrinters",
                           printer_list.size());
    }

    int i = 0;
    for (printing::PrinterList::iterator iter = printer_list.begin();
         iter != printer_list.end(); ++iter, ++i) {
      DictionaryValue* printer_info = new DictionaryValue;
      std::string printerName;
  #if defined(OS_MACOSX)
      // On Mac, |iter->printer_description| specifies the printer name and
      // |iter->printer_name| specifies the device name / printer queue name.
      printerName = iter->printer_description;
  #else
      printerName = iter->printer_name;
  #endif
      printer_info->SetString(printing::kSettingPrinterName, printerName);
      printer_info->SetString(printing::kSettingDeviceName, iter->printer_name);
      printers->Append(printer_info);
    }
    VLOG(1) << "Enumerate printers finished, found " << i << " printers";

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &PrintSystemTaskProxy::SetupPrinterList,
                          printers));
  }

  void SetupPrinterList(ListValue* printers) {
    if (handler_) {
      handler_->SetupPrinterList(*printers);
    }
    delete printers;
  }

  void GetPrinterCapabilities(const std::string& printer_name) {
    VLOG(1) << "Get printer capabilities start for " << printer_name;
    printing::PrinterCapsAndDefaults printer_info;
    bool supports_color = true;
    bool set_duplex_as_default = false;
    if (!print_backend_->GetPrinterCapsAndDefaults(printer_name,
                                                   &printer_info)) {
      return;
    }

#if defined(USE_CUPS)
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

      ppd_choice_t* ch = ppdFindMarkedChoice(ppd, kDuplex);
      if (ch == NULL) {
        ppd_option_t* option = ppdFindOption(ppd, kDuplex);
        if (option != NULL)
          ch = ppdFindChoice(option, option->defchoice);
      }

      if (ch != NULL && strcmp(ch->choice, kDuplexNone) != 0)
        set_duplex_as_default = true;

      ppdClose(ppd);
    }
    file_util::Delete(ppd_file_path, false);
#elif defined(OS_WIN)
    // According to XPS 1.0 spec, only color printers have psk:Color.
    // Therefore we don't need to parse the whole XML file, we just need to
    // search the string.  The spec can be found at:
    // http://msdn.microsoft.com/en-us/windows/hardware/gg463431.
    supports_color = (printer_info.printer_capabilities.find(kPskColor) !=
                      std::string::npos);
    set_duplex_as_default =
        (printer_info.printer_defaults.find(kPskDuplexFeature) !=
            std::string::npos) &&
        (printer_info.printer_defaults.find(kPskTwoSided) !=
            std::string::npos);
#else
  NOTIMPLEMENTED();
#endif

    DictionaryValue settings_info;
    settings_info.SetBoolean(kDisableColorOption, !supports_color);
    if (!supports_color) {
      settings_info.SetBoolean(kSetColorAsDefault, false);
    } else {
       settings_info.SetBoolean(kSetColorAsDefault,
                                PrintPreviewHandler::last_used_color_setting_);
    }
    settings_info.SetBoolean(kSetDuplexAsDefault, set_duplex_as_default);
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

  bool has_logged_printers_count_;

  DISALLOW_COPY_AND_ASSIGN(PrintSystemTaskProxy);
};

// A Task implementation that stores a PDF file on disk.
class PrintToPdfTask : public Task {
 public:
  // Takes ownership of |metafile|.
  PrintToPdfTask(printing::Metafile* metafile, const FilePath& path)
      : metafile_(metafile), path_(path) {
    DCHECK(metafile);
  }

  ~PrintToPdfTask() {
    // This has to get deleted on the same thread it was created.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        new DeleteTask<printing::Metafile>(metafile_.release()));
  }

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
std::string* PrintPreviewHandler::last_used_printer_cloud_print_data_ = NULL;
std::string* PrintPreviewHandler::last_used_printer_name_ = NULL;
bool PrintPreviewHandler::last_used_color_setting_ = false;

PrintPreviewHandler::PrintPreviewHandler()
    : print_backend_(printing::PrintBackend::CreateInstance(NULL)),
      regenerate_preview_request_count_(0),
      manage_printers_dialog_request_count_(0),
      reported_failed_preview_(false),
      has_logged_printers_count_(false) {
  ReportUserActionHistogram(PREVIEW_STARTED);
}

PrintPreviewHandler::~PrintPreviewHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void PrintPreviewHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getDefaultPrinter",
      NewCallback(this, &PrintPreviewHandler::HandleGetDefaultPrinter));
  web_ui_->RegisterMessageCallback("getPrinters",
      NewCallback(this, &PrintPreviewHandler::HandleGetPrinters));
  web_ui_->RegisterMessageCallback("getPreview",
      NewCallback(this, &PrintPreviewHandler::HandleGetPreview));
  web_ui_->RegisterMessageCallback("print",
      NewCallback(this, &PrintPreviewHandler::HandlePrint));
  web_ui_->RegisterMessageCallback("getPrinterCapabilities",
      NewCallback(this, &PrintPreviewHandler::HandleGetPrinterCapabilities));
  web_ui_->RegisterMessageCallback("showSystemDialog",
      NewCallback(this, &PrintPreviewHandler::HandleShowSystemDialog));
  web_ui_->RegisterMessageCallback("morePrinters",
      NewCallback(this, &PrintPreviewHandler::HandleShowSystemDialog));
  web_ui_->RegisterMessageCallback("signIn",
      NewCallback(this, &PrintPreviewHandler::HandleSignin));
  web_ui_->RegisterMessageCallback("addCloudPrinter",
      NewCallback(this, &PrintPreviewHandler::HandleManageCloudPrint));
  web_ui_->RegisterMessageCallback("manageCloudPrinters",
      NewCallback(this, &PrintPreviewHandler::HandleManageCloudPrint));
  web_ui_->RegisterMessageCallback("manageLocalPrinters",
      NewCallback(this, &PrintPreviewHandler::HandleManagePrinters));
  web_ui_->RegisterMessageCallback("reloadCrashedInitiatorTab",
      NewCallback(this, &PrintPreviewHandler::HandleReloadCrashedInitiatorTab));
  web_ui_->RegisterMessageCallback("closePrintPreviewTab",
      NewCallback(this, &PrintPreviewHandler::HandleClosePreviewTab));
  web_ui_->RegisterMessageCallback("hidePreview",
      NewCallback(this, &PrintPreviewHandler::HandleHidePreview));
  web_ui_->RegisterMessageCallback("cancelPendingPrintRequest",
      NewCallback(this, &PrintPreviewHandler::HandleCancelPendingPrintRequest));
  web_ui_->RegisterMessageCallback("saveLastPrinter",
      NewCallback(this, &PrintPreviewHandler::HandleSaveLastPrinter));
}

TabContentsWrapper* PrintPreviewHandler::preview_tab_wrapper() const {
  return TabContentsWrapper::GetCurrentWrapperForContents(preview_tab());
}
TabContents* PrintPreviewHandler::preview_tab() const {
  return web_ui_->tab_contents();
}

void PrintPreviewHandler::HandleGetDefaultPrinter(const ListValue*) {
  scoped_refptr<PrintSystemTaskProxy> task =
      new PrintSystemTaskProxy(AsWeakPtr(),
                               print_backend_.get(),
                               has_logged_printers_count_);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &PrintSystemTaskProxy::GetDefaultPrinter));
}

void PrintPreviewHandler::HandleGetPrinters(const ListValue*) {
  scoped_refptr<PrintSystemTaskProxy> task =
      new PrintSystemTaskProxy(AsWeakPtr(),
                               print_backend_.get(),
                               has_logged_printers_count_);
  has_logged_printers_count_ = true;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &PrintSystemTaskProxy::EnumeratePrinters));
}

void PrintPreviewHandler::HandleGetPreview(const ListValue* args) {
  DCHECK_EQ(3U, args->GetSize());
  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;
  int request_id = -1;
  if (!settings->GetInteger(printing::kPreviewRequestID, &request_id))
    return;

  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->OnPrintPreviewRequest(request_id);
  // Add an additional key in order to identify |print_preview_ui| later on
  // when calling PrintPreviewUI::GetCurrentPrintPreviewStatus() on the IO
  // thread.
  settings->SetString(printing::kPreviewUIAddr,
                      print_preview_ui->GetPrintPreviewUIAddress());

  // Increment request count.
  ++regenerate_preview_request_count_;

  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (!initiator_tab) {
    ReportUserActionHistogram(INITIATOR_TAB_CLOSED);
    print_preview_ui->OnInitiatorTabClosed();
    return;
  }

  // Retrieve the page title and url and send it to the renderer process if
  // headers and footers are to be displayed.
  bool display_header_footer = false;
  if (!settings->GetBoolean(printing::kSettingHeaderFooterEnabled,
                            &display_header_footer)) {
    NOTREACHED();
  }
  if (display_header_footer) {
    settings->SetString(printing::kSettingHeaderFooterTitle,
                        initiator_tab->tab_contents()->GetTitle());
    std::string url;
    NavigationEntry* entry = initiator_tab->controller().GetActiveEntry();
    if (entry)
      url = entry->virtual_url().spec();
    settings->SetString(printing::kSettingHeaderFooterURL, url);
  }

  bool generate_draft_data = false;
  bool success = settings->GetBoolean(printing::kSettingGenerateDraftData,
                                      &generate_draft_data);
  DCHECK(success);

  if (!generate_draft_data) {
    double draft_page_count_double = -1;
    success = args->GetDouble(1, &draft_page_count_double);
    DCHECK(success);
    int draft_page_count = static_cast<int>(draft_page_count_double);

    bool preview_modifiable = false;
    success = args->GetBoolean(2, &preview_modifiable);
    DCHECK(success);

    if (draft_page_count != -1 && preview_modifiable &&
        print_preview_ui->GetAvailableDraftPageCount() != draft_page_count) {
      settings->SetBoolean(printing::kSettingGenerateDraftData, true);
    }
  }

  VLOG(1) << "Print preview request start";
  RenderViewHost* rvh = initiator_tab->render_view_host();
  rvh->Send(new PrintMsg_PrintPreview(rvh->routing_id(), *settings));
}

void PrintPreviewHandler::HandlePrint(const ListValue* args) {
  ReportStats();

  // Record the number of times the user requests to regenerate preview data
  // before printing.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforePrint",
                       regenerate_preview_request_count_);

  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (initiator_tab) {
    RenderViewHost* rvh = initiator_tab->render_view_host();
    rvh->Send(new PrintMsg_ResetScriptedPrintCount(rvh->routing_id()));
  }

  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

  // Storing last used color setting.
  settings->GetBoolean("color", &last_used_color_setting_);

  bool print_to_pdf = false;
  settings->GetBoolean(printing::kSettingPrintToPDF, &print_to_pdf);

  settings->SetBoolean(printing::kSettingHeaderFooterEnabled, false);

  TabContentsWrapper* preview_tab_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(preview_tab());

  bool print_to_cloud = settings->HasKey(printing::kSettingCloudPrintId);
  if (print_to_cloud) {
    std::string print_ticket;
    args->GetString(1, &print_ticket);
    SendCloudPrintJob(*settings, print_ticket);
  } else if (print_to_pdf) {
    ReportUserActionHistogram(PRINT_TO_PDF);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPDF",
                         GetPageCountFromSettingsDictionary(*settings));

    // Pre-populating select file dialog with print job title.
    string16 print_job_title_utf16 =
        preview_tab_wrapper->print_view_manager()->RenderSourceName();

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
    ReportPrintSettingsStats(*settings);
    ReportUserActionHistogram(PRINT_TO_PRINTER);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPrinter",
                         GetPageCountFromSettingsDictionary(*settings));

    // This tries to activate the initiator tab as well, so do not clear the
    // association with the initiator tab yet.
    HidePreviewTab();

    // Do this so the initiator tab can open a new print preview tab.
    ClearInitiatorTabDetails();

    // The PDF being printed contains only the pages that the user selected,
    // so ignore the page range and print all pages.
    settings->Remove(printing::kSettingPageRange, NULL);
    RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
    rvh->Send(new PrintMsg_PrintForPrintPreview(rvh->routing_id(), *settings));
  }
}

void PrintPreviewHandler::HandleHidePreview(const ListValue*) {
  HidePreviewTab();
}

void PrintPreviewHandler::HandleCancelPendingPrintRequest(const ListValue*) {
  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (initiator_tab) {
    ClearInitiatorTabDetails();
  } else {
    // Initiator tab does not exists. Get the wrapper contents of current tab.
    Browser* browser = BrowserList::GetLastActive();
    if (browser)
      initiator_tab = browser->GetSelectedTabContentsWrapper();
  }

  if (initiator_tab)
    initiator_tab->print_view_manager()->PreviewPrintingRequestCancelled();
  delete preview_tab_wrapper();
}

void PrintPreviewHandler::HandleSaveLastPrinter(const ListValue* args) {
  std::string data_to_save;
  if (args->GetString(0, &data_to_save) && !data_to_save.empty()) {
    if (last_used_printer_name_ == NULL)
      last_used_printer_name_ = new std::string();
    *last_used_printer_name_ = data_to_save;
  }
  if (args->GetString(1, &data_to_save) && !data_to_save.empty()) {
    if (last_used_printer_cloud_print_data_ == NULL)
      last_used_printer_cloud_print_data_ = new std::string();
    *last_used_printer_cloud_print_data_ = data_to_save;
  }
}

void PrintPreviewHandler::HandleGetPrinterCapabilities(const ListValue* args) {
  std::string printer_name;
  bool ret = args->GetString(0, &printer_name);
  if (!ret || printer_name.empty())
    return;

  scoped_refptr<PrintSystemTaskProxy> task =
      new PrintSystemTaskProxy(AsWeakPtr(),
                               print_backend_.get(),
                               has_logged_printers_count_);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
                        &PrintSystemTaskProxy::GetPrinterCapabilities,
                        printer_name));
}

void PrintPreviewHandler::HandleSignin(const ListValue*) {
  cloud_print_signin_dialog::CreateCloudPrintSigninDialog(preview_tab());
}

void PrintPreviewHandler::HandleManageCloudPrint(const ListValue*) {
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(CloudPrintURL(browser->profile()).
                   GetCloudPrintServiceManageURL(),
                   GURL(),
                   NEW_FOREGROUND_TAB,
                   PageTransition::LINK);
}

void PrintPreviewHandler::HandleShowSystemDialog(const ListValue*) {
  ReportStats();
  ReportUserActionHistogram(FALLBACK_TO_ADVANCED_SETTINGS_DIALOG);

  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (!initiator_tab)
    return;

  printing::PrintViewManager* manager = initiator_tab->print_view_manager();
  manager->set_observer(this);
  manager->PrintForSystemDialogNow();

  // Cancel the pending preview request if exists.
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->OnCancelPendingPreviewRequest();
}

void PrintPreviewHandler::HandleManagePrinters(const ListValue*) {
  ++manage_printers_dialog_request_count_;
  printing::PrinterManagerDialog::ShowPrinterManagerDialog();
}

void PrintPreviewHandler::HandleReloadCrashedInitiatorTab(const ListValue*) {
  ReportStats();
  ReportUserActionHistogram(INITIATOR_TAB_CRASHED);

  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (!initiator_tab)
    return;

  TabContents* contents = initiator_tab->tab_contents();
  contents->OpenURL(contents->GetURL(), GURL(), CURRENT_TAB,
                    PageTransition::RELOAD);
  ActivateInitiatorTabAndClosePreviewTab();
}

void PrintPreviewHandler::HandleClosePreviewTab(const ListValue*) {
  ReportStats();
  ReportUserActionHistogram(CANCEL);

  // Record the number of times the user requests to regenerate preview data
  // before cancelling.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforeCancel",
                       regenerate_preview_request_count_);

  ActivateInitiatorTabAndClosePreviewTab();
}

void PrintPreviewHandler::ReportStats() {
  UMA_HISTOGRAM_COUNTS("PrintPreview.ManagePrinters",
                       manage_printers_dialog_request_count_);
}

void PrintPreviewHandler::ActivateInitiatorTabAndClosePreviewTab() {
  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (initiator_tab) {
    static_cast<RenderViewHostDelegate*>(
        initiator_tab->tab_contents())->Activate();
  }
  ClosePrintPreviewTab();
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const DictionaryValue& settings_info) {
  VLOG(1) << "Get printer capabilities finished";
  web_ui_->CallJavascriptFunction("updateWithPrinterCapabilities",
                                  settings_info);
}

void PrintPreviewHandler::SendDefaultPrinter(
    const StringValue& default_printer,
    const StringValue& cloud_print_data) {
  web_ui_->CallJavascriptFunction("setDefaultPrinter",
                                  default_printer,
                                  cloud_print_data);
}

void PrintPreviewHandler::SetupPrinterList(const ListValue& printers) {
  SendCloudPrintEnabled();
  web_ui_->CallJavascriptFunction("setPrinters", printers);
}

void PrintPreviewHandler::SendCloudPrintEnabled() {
#if defined(OS_CHROMEOS)
  bool enable_cloud_print_integration = true;
#else
  bool enable_cloud_print_integration =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableCloudPrint);
#endif
  GURL gcp_url(CloudPrintURL(BrowserList::GetLastActive()->profile()).
               GetCloudPrintServiceURL());
  base::FundamentalValue enable(enable_cloud_print_integration);
  base::StringValue gcp_url_value(gcp_url.spec());
  web_ui_->CallJavascriptFunction("setUseCloudPrint", enable, gcp_url_value);
}

void PrintPreviewHandler::SendCloudPrintJob(const DictionaryValue& settings,
                                            std::string print_ticket) {
  scoped_refptr<RefCountedBytes> data;
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  CHECK(data.get());
  DCHECK_GT(data->size(), 0U);

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(preview_tab());
  string16 print_job_title_utf16 =
      wrapper->print_view_manager()->RenderSourceName();
  std::string print_job_title = UTF16ToUTF8(print_job_title_utf16);
  std::string printer_id;
  settings.GetString("cloudPrintID", &printer_id);
// BASE64 encode the job data.
  std::string raw_data(reinterpret_cast<const char*>(data->front()),
                       data->size());
  std::string base64_data;
  if (!base::Base64Encode(raw_data, &base64_data)) {
    NOTREACHED() << "Base64 encoding PDF data.";
  }

  const char boundary[] = "----CloudPrintFormBoundaryjc9wuprokl8i";
  const char prolog[] = "--%s\r\n"
    "Content-Disposition: form-data; name=\"capabilities\"\r\n\r\n%s\r\n"
    "--%s\r\n"
    "Content-Disposition: form-data; name=\"contentType\"\r\n\r\ndataUrl\r\n"
    "--%s\r\n"
    "Content-Disposition: form-data; name=\"title\"\r\n\r\n%s\r\n"
    "--%s\r\n"
    "Content-Disposition: form-data; name=\"printerid\"\r\n\r\n%s\r\n"
    "--%s\r\n"
    "Content-Disposition: form-data; name=\"content\"\r\n\r\n"
    "data:application/pdf;base64,%s\r\n"
    "--%s\r\n";

// TODO(abodenha@chromium.org) This implies a large copy operation.
// Profile this and optimize if necessary.
  std::string final_data;
  base::SStringPrintf(&final_data,
                      prolog,
                      boundary,
                      print_ticket.c_str(),
                      boundary,
                      boundary,
                      print_job_title.c_str(),
                      boundary,
                      printer_id.c_str(),
                      boundary,
                      base64_data.c_str(),
                      boundary);

  StringValue data_value(final_data);

  web_ui_->CallJavascriptFunction("printToCloud",
                                  data_value);
}

TabContentsWrapper* PrintPreviewHandler::GetInitiatorTab() const {
  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return NULL;
  return tab_controller->GetInitiatorTab(preview_tab_wrapper());
}

void PrintPreviewHandler::ClosePrintPreviewTab() {
  TabContentsWrapper* tab =
      TabContentsWrapper::GetCurrentWrapperForContents(preview_tab());
  if (!tab)
    return;
  Browser* preview_tab_browser = BrowserList::FindBrowserWithID(
      tab->restore_tab_helper()->window_id().id());
  if (!preview_tab_browser)
    return;
  TabStripModel* tabstrip = preview_tab_browser->tabstrip_model();
  int index = tabstrip->GetIndexOfTabContents(tab);
  if (index == TabStripModel::kNoTab)
    return;

  // Keep print preview tab out of the recently closed tab list, because
  // re-opening that page will just display a non-functional print preview page.
  tabstrip->CloseTabContentsAt(index, TabStripModel::CLOSE_NONE);
}

void PrintPreviewHandler::OnPrintDialogShown() {
  static_cast<RenderViewHostDelegate*>(
      GetInitiatorTab()->tab_contents())->Activate();
  ClosePrintPreviewTab();
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
      preview_tab(),
      platform_util::GetTopLevel(preview_tab()->GetNativeView()),
      NULL);
}

void PrintPreviewHandler::OnTabDestroyed() {
  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (!initiator_tab)
    return;

  initiator_tab->print_view_manager()->set_observer(NULL);
}

void PrintPreviewHandler::OnPrintPreviewFailed() {
  if (reported_failed_preview_)
    return;
  reported_failed_preview_ = true;
  ReportUserActionHistogram(PREVIEW_FAILED);
}

void PrintPreviewHandler::ShowSystemDialog() {
  HandleShowSystemDialog(NULL);
}

void PrintPreviewHandler::FileSelected(const FilePath& path,
                                       int index, void* params) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  scoped_refptr<RefCountedBytes> data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  if (!data.get()) {
    NOTREACHED();
    return;
  }

  printing::PreviewMetafile* metafile = new printing::PreviewMetafile;
  metafile->InitFromData(static_cast<const void*>(data->front()), data->size());

  // Updating last_saved_path_ to the newly selected folder.
  *last_saved_path_ = path.DirName();

  PrintToPdfTask* task = new PrintToPdfTask(metafile, path);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, task);

  ActivateInitiatorTabAndClosePreviewTab();
}

void PrintPreviewHandler::FileSelectionCanceled(void* params) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->OnFileSelectionCancelled();
}

void PrintPreviewHandler::HidePreviewTab() {
  TabContentsWrapper* preview_tab_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(preview_tab());
  if (GetBackgroundPrintingManager()->HasPrintPreviewTab(preview_tab_wrapper))
    return;
  GetBackgroundPrintingManager()->OwnPrintPreviewTab(preview_tab_wrapper);
}

void PrintPreviewHandler::ClearInitiatorTabDetails() {
  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (!initiator_tab)
    return;

  // We no longer require the initiator tab details. Remove those details
  // associated with the preview tab to allow the initiator tab to create
  // another preview tab.
  printing::PrintPreviewTabController* tab_controller =
     printing::PrintPreviewTabController::GetInstance();
  if (tab_controller)
    tab_controller->EraseInitiatorTabInfo(preview_tab_wrapper());
}
