// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_handler.h"

#include <ctype.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/number_formatting.h"
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
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_system_task_proxy.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/cloud_print_signin_dialog.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/print_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/page_range.h"
#include "printing/page_size_margins.h"
#include "printing/print_settings.h"
#include "unicode/ulocdata.h"

#if !defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

using content::BrowserThread;

namespace {

enum UserActionBuckets {
  PRINT_TO_PRINTER,
  PRINT_TO_PDF,
  CANCEL,
  FALLBACK_TO_ADVANCED_SETTINGS_DIALOG,
  PREVIEW_FAILED,
  PREVIEW_STARTED,
  INITIATOR_TAB_CRASHED,  // UNUSED
  INITIATOR_TAB_CLOSED,
  PRINT_WITH_CLOUD_PRINT,
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

// Name of a dictionary fielad holdong cloud print related data;
const char kCloudPrintData[] = "cloudPrintData";
// Name of a dictionary field holding the initiator tab title.
const char kInitiatorTabTitle[] = "initiatorTabTitle";
// Name of a dictionary field holding the measurement system according to the
// locale.
const char kMeasurementSystem[] = "measurementSystem";
// Name of a dictionary field holding the number format according to the locale.
const char kNumberFormat[] = "numberFormat";


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

  int color_mode;
  if (settings.GetInteger(printing::kSettingColor, &color_mode)) {
    ReportPrintSettingHistogram(
        printing::isColorModelSelected(color_mode) ? COLOR : BLACK_AND_WHITE);
  }
}

}  // namespace

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
printing::ColorModels PrintPreviewHandler::last_used_color_model_ =
    printing::UNKNOWN_COLOR_MODEL;
printing::MarginType PrintPreviewHandler::last_used_margins_type_ =
    printing::DEFAULT_MARGINS;
printing::PageSizeMargins*
    PrintPreviewHandler::last_used_page_size_margins_ = NULL;

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
  web_ui_->RegisterMessageCallback("getPrinters",
      base::Bind(&PrintPreviewHandler::HandleGetPrinters,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("getPreview",
      base::Bind(&PrintPreviewHandler::HandleGetPreview,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("print",
      base::Bind(&PrintPreviewHandler::HandlePrint,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("getPrinterCapabilities",
      base::Bind(&PrintPreviewHandler::HandleGetPrinterCapabilities,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("showSystemDialog",
      base::Bind(&PrintPreviewHandler::HandleShowSystemDialog,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("signIn",
      base::Bind(&PrintPreviewHandler::HandleSignin,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("manageCloudPrinters",
      base::Bind(&PrintPreviewHandler::HandleManageCloudPrint,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("manageLocalPrinters",
      base::Bind(&PrintPreviewHandler::HandleManagePrinters,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("closePrintPreviewTab",
      base::Bind(&PrintPreviewHandler::HandleClosePreviewTab,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("hidePreview",
      base::Bind(&PrintPreviewHandler::HandleHidePreview,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("cancelPendingPrintRequest",
      base::Bind(&PrintPreviewHandler::HandleCancelPendingPrintRequest,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("saveLastPrinter",
      base::Bind(&PrintPreviewHandler::HandleSaveLastPrinter,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("getInitialSettings",
      base::Bind(&PrintPreviewHandler::HandleGetInitialSettings,
                 base::Unretained(this)));
}

TabContentsWrapper* PrintPreviewHandler::preview_tab_wrapper() const {
  return TabContentsWrapper::GetCurrentWrapperForContents(preview_tab());
}
TabContents* PrintPreviewHandler::preview_tab() const {
  return web_ui_->tab_contents();
}

void PrintPreviewHandler::HandleGetPrinters(const ListValue* /*args*/) {
  scoped_refptr<PrintSystemTaskProxy> task =
      new PrintSystemTaskProxy(AsWeakPtr(),
                               print_backend_.get(),
                               has_logged_printers_count_);
  has_logged_printers_count_ = true;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::EnumeratePrinters, task.get()));
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
    print_preview_ui->OnClosePrintPreviewTab();
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
  CHECK(initiator_tab);

  RenderViewHost* init_rvh = initiator_tab->render_view_host();
  init_rvh->Send(new PrintMsg_ResetScriptedPrintCount(init_rvh->routing_id()));

  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

  // Storing last used color model.
  int color_model;
  if (!settings->GetInteger(printing::kSettingColor, &color_model))
    color_model = printing::GRAY;
  last_used_color_model_ = static_cast<printing::ColorModels>(color_model);

  // Storing last used margin settings.
  bool is_modifiable;
  settings->GetBoolean(printing::kSettingPreviewModifiable, &is_modifiable);
  if (is_modifiable) {
    int margin_type;
    if (!settings->GetInteger(printing::kSettingMarginsType, &margin_type))
      margin_type = printing::DEFAULT_MARGINS;
    last_used_margins_type_ = static_cast<printing::MarginType>(margin_type);
    if (last_used_margins_type_ == printing::CUSTOM_MARGINS) {
      if (!last_used_page_size_margins_)
        last_used_page_size_margins_ = new printing::PageSizeMargins();
      GetCustomMarginsFromJobSettings(*settings, last_used_page_size_margins_);
    }
  }

  bool print_to_pdf = false;
  settings->GetBoolean(printing::kSettingPrintToPDF, &print_to_pdf);

  bool open_pdf_in_preview = false;
#if defined(OS_MACOSX)
  open_pdf_in_preview = settings->HasKey(printing::kSettingOpenPDFInPreview);
#endif

  settings->SetBoolean(printing::kSettingHeaderFooterEnabled, false);

  bool is_cloud_printer = settings->HasKey(printing::kSettingCloudPrintId);
  bool is_cloud_dialog = false;
  settings->GetBoolean(printing::kSettingCloudPrintDialog, &is_cloud_dialog);
  if (is_cloud_printer && !open_pdf_in_preview) {
    std::string print_ticket;
    args->GetString(1, &print_ticket);
    SendCloudPrintJob(*settings, print_ticket);
  } else if (print_to_pdf && !open_pdf_in_preview) {
    HandlePrintToPdf(*settings);
  } else if (is_cloud_dialog && !open_pdf_in_preview) {
    HandlePrintWithCloudPrint();
  } else {
    ReportPrintSettingsStats(*settings);
    ReportUserActionHistogram(PRINT_TO_PRINTER);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPrinter",
                         GetPageCountFromSettingsDictionary(*settings));

    // This tries to activate the initiator tab as well, so do not clear the
    // association with the initiator tab yet.
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
    print_preview_ui->OnHidePreviewTab();

    // Do this so the initiator tab can open a new print preview tab.
    ClearInitiatorTabDetails();

    // The PDF being printed contains only the pages that the user selected,
    // so ignore the page range and print all pages.
    settings->Remove(printing::kSettingPageRange, NULL);
    RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
    rvh->Send(new PrintMsg_PrintForPrintPreview(rvh->routing_id(), *settings));
  }
  initiator_tab->print_view_manager()->PrintPreviewDone();
}

void PrintPreviewHandler::HandlePrintToPdf(
    const base::DictionaryValue& settings) {
  if (print_to_pdf_path_.get()) {
    // User has already selected a path, no need to show the dialog again.
    PostPrintToPdfTask();
  } else if (!select_file_dialog_.get() || !select_file_dialog_->IsRunning(
        platform_util::GetTopLevel(preview_tab()->GetNativeView()))) {
    ReportUserActionHistogram(PRINT_TO_PDF);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPDF",
                         GetPageCountFromSettingsDictionary(settings));

    // Pre-populating select file dialog with print job title.
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
    string16 print_job_title_utf16 = print_preview_ui->initiator_tab_title();

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
  }
}

void PrintPreviewHandler::HandleHidePreview(const ListValue* /*args*/) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->OnHidePreviewTab();
}

void PrintPreviewHandler::HandleCancelPendingPrintRequest(
    const ListValue* /*args*/) {
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
      base::Bind(&PrintSystemTaskProxy::GetPrinterCapabilities, task.get(),
                 printer_name));
}

void PrintPreviewHandler::HandleSignin(const ListValue* /*args*/) {
  cloud_print_signin_dialog::CreateCloudPrintSigninDialog(preview_tab());
}

void PrintPreviewHandler::HandlePrintWithCloudPrint() {
  // Record the number of times the user asks to print via cloud print
  // instead of the print preview dialog.
  ReportStats();
  ReportUserActionHistogram(PRINT_WITH_CLOUD_PRINT);

  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  scoped_refptr<RefCountedBytes> data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  CHECK(data.get());
  DCHECK_GT(data->size(), 0U);
  print_dialog_cloud::CreatePrintDialogForBytes(data,
      string16(print_preview_ui->initiator_tab_title()),
      string16(),
      std::string("application/pdf"),
      true);

  // Once the cloud print dialog comes up we're no longer in a background
  // printing situation.  Close the print preview.
  // TODO(abodenha@chromium.org) The flow should be changed as described in
  // http://code.google.com/p/chromium/issues/detail?id=44093
  ActivateInitiatorTabAndClosePreviewTab();
}

void PrintPreviewHandler::HandleManageCloudPrint(const ListValue* /*args*/) {
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(CloudPrintURL(browser->profile()).
                   GetCloudPrintServiceManageURL(),
                   GURL(),
                   NEW_FOREGROUND_TAB,
                   content::PAGE_TRANSITION_LINK);
}

void PrintPreviewHandler::HandleShowSystemDialog(const ListValue* /*args*/) {
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

void PrintPreviewHandler::HandleManagePrinters(const ListValue* /*args*/) {
  ++manage_printers_dialog_request_count_;
  printing::PrinterManagerDialog::ShowPrinterManagerDialog();
}

void PrintPreviewHandler::HandleClosePreviewTab(const ListValue* /*args*/) {
  ReportStats();
  ReportUserActionHistogram(CANCEL);

  // Record the number of times the user requests to regenerate preview data
  // before cancelling.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforeCancel",
                       regenerate_preview_request_count_);
}

void PrintPreviewHandler::ReportStats() {
  UMA_HISTOGRAM_COUNTS("PrintPreview.ManagePrinters",
                       manage_printers_dialog_request_count_);
}

void PrintPreviewHandler::GetNumberFormatAndMeasurementSystem(
    base::DictionaryValue* settings) {

  // Getting the measurement system based on the locale.
  UErrorCode errorCode = U_ZERO_ERROR;
  const char* locale = g_browser_process->GetApplicationLocale().c_str();
  UMeasurementSystem system = ulocdata_getMeasurementSystem(locale, &errorCode);
  if (errorCode > U_ZERO_ERROR || system == UMS_LIMIT)
    system = UMS_SI;

  // Getting the number formatting based on the locale and writing to
  // dictionary.
  settings->SetString(kNumberFormat, base::FormatDouble(123456.78, 2));
  settings->SetInteger(kMeasurementSystem, system);
}

void PrintPreviewHandler::GetLastUsedMarginSettings(
    base::DictionaryValue* custom_margins) {
  custom_margins->SetInteger(printing::kSettingMarginsType,
                             PrintPreviewHandler::last_used_margins_type_);
  if (last_used_page_size_margins_) {
    custom_margins->SetDouble(printing::kSettingMarginTop,
                              last_used_page_size_margins_->margin_top);
    custom_margins->SetDouble(printing::kSettingMarginBottom,
                              last_used_page_size_margins_->margin_bottom);
    custom_margins->SetDouble(printing::kSettingMarginLeft,
                              last_used_page_size_margins_->margin_left);
    custom_margins->SetDouble(printing::kSettingMarginRight,
                              last_used_page_size_margins_->margin_right);
  }
}

void PrintPreviewHandler::HandleGetInitialSettings(const ListValue* /*args*/) {
  scoped_refptr<PrintSystemTaskProxy> task =
      new PrintSystemTaskProxy(AsWeakPtr(),
                               print_backend_.get(),
                               has_logged_printers_count_);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::GetDefaultPrinter, task.get()));
}

void PrintPreviewHandler::SendInitialSettings(
    const std::string& default_printer,
    const std::string& cloud_print_data) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);

  base::DictionaryValue initial_settings;
  initial_settings.SetString(kInitiatorTabTitle,
                             print_preview_ui->initiator_tab_title());
  initial_settings.SetBoolean(printing::kSettingPreviewModifiable,
                              print_preview_ui->source_is_modifiable());
  initial_settings.SetString(printing::kSettingPrinterName,
                             default_printer);
  initial_settings.SetString(kCloudPrintData, cloud_print_data);

  if (print_preview_ui->source_is_modifiable()) {
    GetLastUsedMarginSettings(&initial_settings);
    GetNumberFormatAndMeasurementSystem(&initial_settings);
  }
  web_ui_->CallJavascriptFunction("setInitialSettings", initial_settings);
}

void PrintPreviewHandler::ActivateInitiatorTabAndClosePreviewTab() {
  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (initiator_tab) {
    static_cast<RenderViewHostDelegate*>(
        initiator_tab->tab_contents())->Activate();
  }
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->OnClosePrintPreviewTab();
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const DictionaryValue& settings_info) {
  VLOG(1) << "Get printer capabilities finished";
  web_ui_->CallJavascriptFunction("updateWithPrinterCapabilities",
                                  settings_info);
}

void PrintPreviewHandler::SetupPrinterList(const ListValue& printers) {
  SendCloudPrintEnabled();
  web_ui_->CallJavascriptFunction("setPrinters", printers);
}

void PrintPreviewHandler::SendCloudPrintEnabled() {
  Profile* profile = BrowserList::GetLastActive()->profile();
  PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(prefs::kCloudPrintSubmitEnabled)) {
    GURL gcp_url(CloudPrintURL(profile).GetCloudPrintServiceURL());
    base::StringValue gcp_url_value(gcp_url.spec());
    web_ui_->CallJavascriptFunction("setUseCloudPrint", gcp_url_value);
  }
}

void PrintPreviewHandler::SendCloudPrintJob(const DictionaryValue& settings,
                                            std::string print_ticket) {
  scoped_refptr<RefCountedBytes> data;
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  CHECK(data.get());
  DCHECK_GT(data->size(), 0U);

  string16 print_job_title_utf16 =
      preview_tab_wrapper()->print_view_manager()->RenderSourceName();
  std::string print_job_title = UTF16ToUTF8(print_job_title_utf16);
  std::string printer_id;
  settings.GetString(printing::kSettingCloudPrintId, &printer_id);
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

void PrintPreviewHandler::OnPrintDialogShown() {
  ActivateInitiatorTabAndClosePreviewTab();
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
  // Updating last_saved_path_ to the newly selected folder.
  *last_saved_path_ = path.DirName();

  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->CallJavascriptFunction("fileSelectionCompleted");
  scoped_refptr<RefCountedBytes> data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  print_to_pdf_path_.reset(new FilePath(path));
  if (data.get())
    PostPrintToPdfTask();
}

void PrintPreviewHandler::PostPrintToPdfTask() {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  scoped_refptr<RefCountedBytes> data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  DCHECK(data.get());
  printing::PreviewMetafile* metafile = new printing::PreviewMetafile;
  metafile->InitFromData(static_cast<const void*>(data->front()), data->size());
  PrintToPdfTask* task = new PrintToPdfTask(metafile,
                                            *print_to_pdf_path_);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, task);
  print_to_pdf_path_.reset();
  ActivateInitiatorTabAndClosePreviewTab();
}

void PrintPreviewHandler::FileSelectionCanceled(void* params) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(web_ui_);
  print_preview_ui->OnFileSelectionCancelled();
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
