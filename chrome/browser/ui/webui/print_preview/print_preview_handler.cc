// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <ctype.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/print_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/page_range.h"
#include "printing/page_size_margins.h"
#include "printing/print_settings.h"
#include "unicode/ulocdata.h"

#if !defined(OS_MACOSX)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

using content::BrowserThread;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderViewHost;
using content::Referrer;
using content::WebContents;
using printing::Metafile;

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
// Name of a dictionary field specifying whether to print automatically in
// kiosk mode. See http://crbug.com/31395.
const char kPrintAutomaticallyInKioskMode[] = "printAutomaticallyInKioskMode";


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
      base::JSONReader::Read(json_str)));
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

// Callback that stores a PDF file on disk.
void PrintToPdfCallback(Metafile* metafile, const FilePath& path) {
  metafile->SaveTo(path);
  // |metafile| must be deleted on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&base::DeletePointer<Metafile>, metafile));
}

static base::LazyInstance<printing::StickySettings> sticky_settings =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
printing::StickySettings* PrintPreviewHandler::GetStickySettings() {
  return sticky_settings.Pointer();
}

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
  web_ui()->RegisterMessageCallback("getPrinters",
      base::Bind(&PrintPreviewHandler::HandleGetPrinters,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getPreview",
      base::Bind(&PrintPreviewHandler::HandleGetPreview,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("print",
      base::Bind(&PrintPreviewHandler::HandlePrint,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getPrinterCapabilities",
      base::Bind(&PrintPreviewHandler::HandleGetPrinterCapabilities,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showSystemDialog",
      base::Bind(&PrintPreviewHandler::HandleShowSystemDialog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("signIn",
      base::Bind(&PrintPreviewHandler::HandleSignin,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("manageCloudPrinters",
      base::Bind(&PrintPreviewHandler::HandleManageCloudPrint,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("manageLocalPrinters",
      base::Bind(&PrintPreviewHandler::HandleManagePrinters,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("closePrintPreviewTab",
      base::Bind(&PrintPreviewHandler::HandleClosePreviewTab,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("hidePreview",
      base::Bind(&PrintPreviewHandler::HandleHidePreview,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("cancelPendingPrintRequest",
      base::Bind(&PrintPreviewHandler::HandleCancelPendingPrintRequest,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("saveLastPrinter",
      base::Bind(&PrintPreviewHandler::HandleSaveLastPrinter,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getInitialSettings",
      base::Bind(&PrintPreviewHandler::HandleGetInitialSettings,
                 base::Unretained(this)));
}

TabContentsWrapper* PrintPreviewHandler::preview_tab_wrapper() const {
  return TabContentsWrapper::GetCurrentWrapperForContents(preview_tab());
}

WebContents* PrintPreviewHandler::preview_tab() const {
  return web_ui()->GetWebContents();
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

  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
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
                        initiator_tab->web_contents()->GetTitle());
    std::string url;
    NavigationEntry* entry =
        initiator_tab->web_contents()->GetController().GetActiveEntry();
    if (entry)
      url = entry->GetVirtualURL().spec();
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
  RenderViewHost* rvh = initiator_tab->web_contents()->GetRenderViewHost();
  rvh->Send(new PrintMsg_PrintPreview(rvh->GetRoutingID(), *settings));
}

void PrintPreviewHandler::HandlePrint(const ListValue* args) {
  ReportStats();

  // Record the number of times the user requests to regenerate preview data
  // before printing.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforePrint",
                       regenerate_preview_request_count_);

  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (initiator_tab) {
    RenderViewHost* rvh = initiator_tab->web_contents()->GetRenderViewHost();
    rvh->Send(new PrintMsg_ResetScriptedPrintCount(rvh->GetRoutingID()));
  }

  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

  // Storing last used settings.
  GetStickySettings()->Store(*settings);

  // Never try to add headers/footers here. It's already in the generated PDF.
  settings->SetBoolean(printing::kSettingHeaderFooterEnabled, false);

  bool print_to_pdf = false;
  bool is_cloud_printer = false;
  bool is_cloud_dialog = false;

  bool open_pdf_in_preview = false;
#if defined(OS_MACOSX)
  open_pdf_in_preview = settings->HasKey(printing::kSettingOpenPDFInPreview);
#endif

  if (!open_pdf_in_preview) {
    settings->GetBoolean(printing::kSettingPrintToPDF, &print_to_pdf);
    settings->GetBoolean(printing::kSettingCloudPrintDialog, &is_cloud_dialog);
    is_cloud_printer = settings->HasKey(printing::kSettingCloudPrintId);
  }

  if (is_cloud_printer) {
    std::string print_ticket;
    bool res = args->GetString(1, &print_ticket);
    DCHECK(res);
    SendCloudPrintJob(*settings, print_ticket);
  } else if (print_to_pdf) {
    HandlePrintToPdf(*settings);
  } else if (is_cloud_dialog) {
    HandlePrintWithCloudPrint();
  } else {
    ReportPrintSettingsStats(*settings);
    ReportUserActionHistogram(PRINT_TO_PRINTER);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPrinter",
                         GetPageCountFromSettingsDictionary(*settings));

    // This tries to activate the initiator tab as well, so do not clear the
    // association with the initiator tab yet.
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        web_ui()->GetController());
    print_preview_ui->OnHidePreviewTab();

    // Do this so the initiator tab can open a new print preview tab.
    ClearInitiatorTabDetails();

    // The PDF being printed contains only the pages that the user selected,
    // so ignore the page range and print all pages.
    settings->Remove(printing::kSettingPageRange, NULL);
    RenderViewHost* rvh = web_ui()->GetWebContents()->GetRenderViewHost();
    rvh->Send(
        new PrintMsg_PrintForPrintPreview(rvh->GetRoutingID(), *settings));

    // For all other cases above, the tab will stay open until the printing has
    // finished. Then the tab closes and PrintPreviewDone() gets called. Here,
    // since we are hiding the tab, and not closing it, we need to make this
    // call.
    if (initiator_tab)
      initiator_tab->print_view_manager()->PrintPreviewDone();
  }
}

void PrintPreviewHandler::HandlePrintToPdf(
    const base::DictionaryValue& settings) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  if (print_to_pdf_path_.get()) {
    // User has already selected a path, no need to show the dialog again.
    scoped_refptr<base::RefCountedBytes> data;
    print_preview_ui->GetPrintPreviewDataForIndex(
        printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
    PostPrintToPdfTask(data);
  } else if (!select_file_dialog_.get() || !select_file_dialog_->IsRunning(
        platform_util::GetTopLevel(preview_tab()->GetNativeView()))) {
    ReportUserActionHistogram(PRINT_TO_PDF);
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPDF",
                         GetPageCountFromSettingsDictionary(settings));

    // Pre-populating select file dialog with print job title.
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
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
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
  if (args->GetString(0, &data_to_save) && !data_to_save.empty())
    GetStickySettings()->StorePrinterName(data_to_save);

  if (args->GetString(1, &data_to_save) && !data_to_save.empty())
    GetStickySettings()->StoreCloudPrintData(data_to_save);
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

// static
void PrintPreviewHandler::OnSigninComplete(
    const base::WeakPtr<PrintPreviewHandler>& handler) {
  if (handler.get()) {
    PrintPreviewUI* print_preview_ui =
        static_cast<PrintPreviewUI*>(handler->web_ui()->GetController());
    if (print_preview_ui)
      print_preview_ui->OnReloadPrintersList();
  }
}

void PrintPreviewHandler::HandleSignin(const ListValue* /*args*/) {
  print_dialog_cloud::CreateCloudPrintSigninDialog(
      base::Bind(&PrintPreviewHandler::OnSigninComplete, AsWeakPtr()));
}

void PrintPreviewHandler::HandlePrintWithCloudPrint() {
  // Record the number of times the user asks to print via cloud print
  // instead of the print preview dialog.
  ReportStats();
  ReportUserActionHistogram(PRINT_WITH_CLOUD_PRINT);

  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  scoped_refptr<base::RefCountedBytes> data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  if (!data.get()) {
    NOTREACHED();
    return;
  }
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
  if (browser != NULL)
    browser->OpenURL(OpenURLParams(
        CloudPrintURL(browser->profile()).GetCloudPrintServiceManageURL(),
        Referrer(),
        NEW_FOREGROUND_TAB,
        content::PAGE_TRANSITION_LINK,
        false));
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
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
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
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());

  base::DictionaryValue initial_settings;
  initial_settings.SetString(kInitiatorTabTitle,
                             print_preview_ui->initiator_tab_title());
  initial_settings.SetBoolean(printing::kSettingPreviewModifiable,
                              print_preview_ui->source_is_modifiable());
  initial_settings.SetString(printing::kSettingPrinterName,
                             default_printer);
  initial_settings.SetString(kCloudPrintData, cloud_print_data);
  initial_settings.SetBoolean(printing::kSettingHeaderFooterEnabled,
                              GetStickySettings()->headers_footers());
  initial_settings.SetInteger(printing::kSettingDuplexMode,
                              GetStickySettings()->duplex_mode());


#if defined(OS_MACOSX)
  bool kiosk_mode = false;  // No kiosk mode on Mac yet.
#else
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  bool kiosk_mode = (cmdline->HasSwitch(switches::kKioskMode) &&
                     cmdline->HasSwitch(switches::kKioskModePrinting));
#endif
  initial_settings.SetBoolean(kPrintAutomaticallyInKioskMode, kiosk_mode);

  if (print_preview_ui->source_is_modifiable()) {
    GetStickySettings()->GetLastUsedMarginSettings(&initial_settings);
    GetNumberFormatAndMeasurementSystem(&initial_settings);
  }
  web_ui()->CallJavascriptFunction("setInitialSettings", initial_settings);
}

void PrintPreviewHandler::ActivateInitiatorTabAndClosePreviewTab() {
  TabContentsWrapper* initiator_tab = GetInitiatorTab();
  if (initiator_tab)
    initiator_tab->web_contents()->GetRenderViewHost()->
        GetDelegate()->Activate();
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  print_preview_ui->OnClosePrintPreviewTab();
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const DictionaryValue& settings_info) {
  VLOG(1) << "Get printer capabilities finished";
  web_ui()->CallJavascriptFunction("updateWithPrinterCapabilities",
                                   settings_info);
}

void PrintPreviewHandler::SetupPrinterList(const ListValue& printers) {
  SendCloudPrintEnabled();
  web_ui()->CallJavascriptFunction("setPrinters", printers);
}

void PrintPreviewHandler::SendCloudPrintEnabled() {
  Browser* browser = BrowserList::GetLastActive();
  if (browser != NULL) {
    Profile* profile = browser->profile();
    PrefService* prefs = profile->GetPrefs();
    if (prefs->GetBoolean(prefs::kCloudPrintSubmitEnabled)) {
      GURL gcp_url(CloudPrintURL(profile).GetCloudPrintServiceURL());
      base::StringValue gcp_url_value(gcp_url.spec());
      web_ui()->CallJavascriptFunction("setUseCloudPrint", gcp_url_value);
    }
  }
}

void PrintPreviewHandler::SendCloudPrintJob(const DictionaryValue& settings,
                                            std::string print_ticket) {
  scoped_refptr<base::RefCountedBytes> data;
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  if (data->size() > 0U && data->front()) {
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

    web_ui()->CallJavascriptFunction("printToCloud", data_value);
  } else {
    NOTREACHED();
  }
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

  // Initializing save_path_ if it is not already initialized.
  if (!GetStickySettings()->save_path()) {
    // Allowing IO operation temporarily. It is ok to do so here because
    // the select file dialog performs IO anyway in order to display the
    // folders and also it is modal.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    FilePath file_path;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &file_path);
    GetStickySettings()->StoreSavePath(file_path);
  }

  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_SAVEAS_FILE,
      string16(),
      GetStickySettings()->save_path()->Append(default_filename),
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
  // Updating |save_path_| to the newly selected folder.
  GetStickySettings()->StoreSavePath(path.DirName());

  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  print_preview_ui->web_ui()->CallJavascriptFunction("fileSelectionCompleted");
  scoped_refptr<base::RefCountedBytes> data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &data);
  print_to_pdf_path_.reset(new FilePath(path));
  if (data.get())
    PostPrintToPdfTask(data);
}

void PrintPreviewHandler::PostPrintToPdfTask(
    scoped_refptr<base::RefCountedBytes> data) {
  printing::PreviewMetafile* metafile = new printing::PreviewMetafile;
  metafile->InitFromData(static_cast<const void*>(data->front()), data->size());
  // PrintToPdfCallback takes ownership of |metafile|.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&PrintToPdfCallback, metafile,
                                     *print_to_pdf_path_));
  print_to_pdf_path_.reset();
  ActivateInitiatorTabAndClosePreviewTab();
}

void PrintPreviewHandler::FileSelectionCanceled(void* params) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
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
