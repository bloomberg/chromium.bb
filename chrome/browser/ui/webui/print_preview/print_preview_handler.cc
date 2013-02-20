// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <ctype.h>

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_system_task_proxy.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/print_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/page_range.h"
#include "printing/page_size_margins.h"
#include "printing/print_settings.h"
#include "third_party/icu/public/i18n/unicode/ulocdata.h"

#if defined(OS_CHROMEOS)
// TODO(kinaba): provide more non-intrusive way for handling local/remote
// distinction and remove these ugly #ifdef's. http://crbug.com/140425
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
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

enum UiBucketGroups {
  DESTINATION_SEARCH,
  GCP_PROMO,
  UI_BUCKET_GROUP_BOUNDARY
};

enum PrintDestinationBuckets {
  DESTINATION_SHOWN,
  DESTINATION_CLOSED_CHANGED,
  DESTINATION_CLOSED_UNCHANGED,
  SIGNIN_PROMPT,
  SIGNIN_TRIGGERED,
  PRINT_DESTINATION_BUCKET_BOUNDARY
};

enum GcpPromoBuckets {
  PROMO_SHOWN,
  PROMO_CLOSED,
  PROMO_CLICKED,
  GCP_PROMO_BUCKET_BOUNDARY
};

void ReportUserActionHistogram(enum UserActionBuckets event) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.UserAction", event,
                            USERACTION_BUCKET_BOUNDARY);
}

void ReportPrintSettingHistogram(enum PrintSettingsBuckets setting) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.PrintSettings", setting,
                            PRINT_SETTINGS_BUCKET_BOUNDARY);
}

void ReportPrintDestinationHistogram(enum PrintDestinationBuckets event) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.DestinationAction", event,
                            PRINT_DESTINATION_BUCKET_BOUNDARY);
}

void ReportGcpPromoHistogram(enum GcpPromoBuckets event) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.GcpPromo", event,
                            GCP_PROMO_BUCKET_BOUNDARY);
}

// Name of a dictionary field holding cloud print related data;
const char kAppState[] = "appState";
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
// Name of a dictionary field holding the state of selection for document.
const char kDocumentHasSelection[] = "documentHasSelection";


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

void ReportPageCount(int page_count, const std::string& printer_type) {
  UMA_HISTOGRAM_COUNTS(base::StringPrintf("PrintPreview.PageCount.%s",
                                          printer_type.c_str()),
                       page_count);
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
void PrintToPdfCallback(Metafile* metafile, const base::FilePath& path) {
  metafile->SaveTo(path);
  // |metafile| must be deleted on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&base::DeletePointer<Metafile>, metafile));
}

#ifdef OS_CHROMEOS
void PrintToPdfCallbackWithCheck(Metafile* metafile,
                                 drive::DriveFileError error,
                                 const base::FilePath& path) {
  if (error != drive::DRIVE_FILE_OK) {
    LOG(ERROR) << "Save to pdf failed to write: " << error;
  } else {
    metafile->SaveTo(path);
  }
  // |metafile| must be deleted on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&base::DeletePointer<Metafile>, metafile));
}
#endif

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
      manage_cloud_printers_dialog_request_count_(0),
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
  web_ui()->RegisterMessageCallback("closePrintPreviewDialog",
      base::Bind(&PrintPreviewHandler::HandleClosePreviewDialog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("hidePreview",
      base::Bind(&PrintPreviewHandler::HandleHidePreview,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("cancelPendingPrintRequest",
      base::Bind(&PrintPreviewHandler::HandleCancelPendingPrintRequest,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("saveAppState",
      base::Bind(&PrintPreviewHandler::HandleSaveAppState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getInitialSettings",
      base::Bind(&PrintPreviewHandler::HandleGetInitialSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("reportUiEvent",
      base::Bind(&PrintPreviewHandler::HandleReportUiEvent,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("printWithCloudPrint",
      base::Bind(&PrintPreviewHandler::HandlePrintWithCloudPrint,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("forceOpenNewTab",
        base::Bind(&PrintPreviewHandler::HandleForceOpenNewTab,
                   base::Unretained(this)));
}

WebContents* PrintPreviewHandler::preview_web_contents() const {
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
  settings->SetInteger(printing::kPreviewUIID,
                       print_preview_ui->GetIDForPrintPreviewUI());

  // Increment request count.
  ++regenerate_preview_request_count_;

  WebContents* initiator_tab = GetInitiatorTab();
  if (!initiator_tab) {
    ReportUserActionHistogram(INITIATOR_TAB_CLOSED);
    print_preview_ui->OnClosePrintPreviewDialog();
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
                        initiator_tab->GetTitle());
    std::string url;
    NavigationEntry* entry = initiator_tab->GetController().GetActiveEntry();
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
  RenderViewHost* rvh = initiator_tab->GetRenderViewHost();
  rvh->Send(new PrintMsg_PrintPreview(rvh->GetRoutingID(), *settings));
}

void PrintPreviewHandler::HandlePrint(const ListValue* args) {
  ReportStats();

  // Record the number of times the user requests to regenerate preview data
  // before printing.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforePrint",
                       regenerate_preview_request_count_);

  WebContents* initiator_tab = GetInitiatorTab();
  if (initiator_tab) {
    RenderViewHost* rvh = initiator_tab->GetRenderViewHost();
    rvh->Send(new PrintMsg_ResetScriptedPrintCount(rvh->GetRoutingID()));
  }

  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

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

  int page_count = 0;
  settings->GetInteger(printing::kSettingPreviewPageCount, &page_count);

  if (print_to_pdf) {
    ReportPageCount(page_count, "PrintToPDF");
    ReportUserActionHistogram(PRINT_TO_PDF);
    PrintToPdf();
    return;
  }

  scoped_refptr<base::RefCountedBytes> data;
  string16 title;
  if (!GetPreviewDataAndTitle(&data, &title)) {
    // Nothing to print, no preview available.
    return;
  }

  if (is_cloud_printer) {
    ReportPageCount(page_count, "PrintToCloudPrint");
    ReportUserActionHistogram(PRINT_WITH_CLOUD_PRINT);
    SendCloudPrintJob(data);
  } else if (is_cloud_dialog) {
    ReportPageCount(page_count, "PrintToCloudPrintWebDialog");
    PrintWithCloudPrintDialog(data, title);
  } else {
    ReportPageCount(page_count, "PrintToPrinter");
    ReportUserActionHistogram(PRINT_TO_PRINTER);
    ReportPrintSettingsStats(*settings);

    // This tries to activate the initiator tab as well, so do not clear the
    // association with the initiator tab yet.
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        web_ui()->GetController());
    print_preview_ui->OnHidePreviewDialog();

    // Do this so the initiator tab can open a new print preview dialog, while
    // the current print preview dialog is still handling its print job.
    ClearInitiatorTabDetails();

    // The PDF being printed contains only the pages that the user selected,
    // so ignore the page range and print all pages.
    settings->Remove(printing::kSettingPageRange, NULL);
    // Set ID to know whether printing is for preview.
    settings->SetInteger(printing::kPreviewUIID,
                         print_preview_ui->GetIDForPrintPreviewUI());
    RenderViewHost* rvh = preview_web_contents()->GetRenderViewHost();
    rvh->Send(new PrintMsg_PrintForPrintPreview(rvh->GetRoutingID(),
                                                *settings));

    // For all other cases above, the preview dialog will stay open until the
    // printing has finished. Then the dialog closes and PrintPreviewDone() gets
    // called. In the case below, since the preview dialog will be hidden and
    // not closed, we need to make this call.
    if (initiator_tab) {
      printing::PrintViewManager* print_view_manager =
          printing::PrintViewManager::FromWebContents(initiator_tab);
      print_view_manager->PrintPreviewDone();
    }
  }
}

void PrintPreviewHandler::PrintToPdf() {
  if (print_to_pdf_path_.get()) {
    // User has already selected a path, no need to show the dialog again.
    PostPrintToPdfTask();
  } else if (!select_file_dialog_ ||
             !select_file_dialog_->IsRunning(
                  platform_util::GetTopLevel(
                      preview_web_contents()->GetNativeView()))) {
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        web_ui()->GetController());
    // Pre-populating select file dialog with print job title.
    string16 print_job_title_utf16 = print_preview_ui->initiator_tab_title();

#if defined(OS_WIN)
    base::FilePath::StringType print_job_title(print_job_title_utf16);
#elif defined(OS_POSIX)
    base::FilePath::StringType print_job_title =
        UTF16ToUTF8(print_job_title_utf16);
#endif

    file_util::ReplaceIllegalCharactersInPath(&print_job_title, '_');
    base::FilePath default_filename(print_job_title);
    default_filename =
        default_filename.ReplaceExtension(FILE_PATH_LITERAL("pdf"));

    SelectFile(default_filename);
  }
}

void PrintPreviewHandler::HandleHidePreview(const ListValue* /*args*/) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  print_preview_ui->OnHidePreviewDialog();
}

void PrintPreviewHandler::HandleCancelPendingPrintRequest(
    const ListValue* /*args*/) {
  WebContents* initiator_tab = GetInitiatorTab();
  if (initiator_tab)
    ClearInitiatorTabDetails();
  gfx::NativeWindow parent = initiator_tab ?
      initiator_tab->GetView()->GetTopLevelNativeWindow() :
      NULL;
  chrome::ShowPrintErrorDialog(parent);
}

void PrintPreviewHandler::HandleSaveAppState(const ListValue* args) {
  std::string data_to_save;
  printing::StickySettings* sticky_settings = GetStickySettings();
  if (args->GetString(0, &data_to_save) && !data_to_save.empty())
    sticky_settings->StoreAppState(data_to_save);
  sticky_settings->SaveInPrefs(Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext())->GetPrefs());
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
  gfx::NativeWindow modal_parent =
      platform_util::GetTopLevel(preview_web_contents()->GetNativeView());
  print_dialog_cloud::CreateCloudPrintSigninDialog(
      preview_web_contents()->GetBrowserContext(),
      modal_parent,
      base::Bind(&PrintPreviewHandler::OnSigninComplete, AsWeakPtr()));
}

void PrintPreviewHandler::PrintWithCloudPrintDialog(
    const base::RefCountedBytes* data,
    const string16& title) {
  // Record the number of times the user asks to print via cloud print
  // instead of the print preview dialog.
  ReportStats();

  gfx::NativeWindow modal_parent =
      platform_util::GetTopLevel(preview_web_contents()->GetNativeView());
  print_dialog_cloud::CreatePrintDialogForBytes(
      preview_web_contents()->GetBrowserContext(),
      modal_parent,
      data,
      title,
      string16(),
      std::string("application/pdf"));

  // Once the cloud print dialog comes up we're no longer in a background
  // printing situation.  Close the print preview.
  // TODO(abodenha@chromium.org) The flow should be changed as described in
  // http://code.google.com/p/chromium/issues/detail?id=44093
  ClosePreviewDialogAndActivateInitiatorTab();
}

void PrintPreviewHandler::HandleManageCloudPrint(const ListValue* /*args*/) {
  ++manage_cloud_printers_dialog_request_count_;
  Profile* profile = Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  preview_web_contents()->OpenURL(
      OpenURLParams(
          CloudPrintURL(profile).GetCloudPrintServiceManageURL(),
          Referrer(),
          NEW_FOREGROUND_TAB,
          content::PAGE_TRANSITION_LINK,
          false));
}

void PrintPreviewHandler::HandleShowSystemDialog(const ListValue* /*args*/) {
  ReportStats();
  ReportUserActionHistogram(FALLBACK_TO_ADVANCED_SETTINGS_DIALOG);

  WebContents* initiator_tab = GetInitiatorTab();
  if (!initiator_tab)
    return;

  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->set_observer(this);
  print_view_manager->PrintForSystemDialogNow();

  // Cancel the pending preview request if exists.
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  print_preview_ui->OnCancelPendingPreviewRequest();
}

void PrintPreviewHandler::HandleManagePrinters(const ListValue* /*args*/) {
  ++manage_printers_dialog_request_count_;
  printing::PrinterManagerDialog::ShowPrinterManagerDialog();
}

void PrintPreviewHandler::HandlePrintWithCloudPrint(
    const base::ListValue* /*args*/) {
  scoped_refptr<base::RefCountedBytes> data;
  string16 title;
  if (!GetPreviewDataAndTitle(&data, &title)) {
    // Nothing to print, no preview available.
    return;
  }
  PrintWithCloudPrintDialog(data, title);
}

void PrintPreviewHandler::HandleClosePreviewDialog(const ListValue* /*args*/) {
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
  UMA_HISTOGRAM_COUNTS("PrintPreview.ManageCloudPrinters",
                       manage_cloud_printers_dialog_request_count_);
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
  SendCloudPrintEnabled();
}

void PrintPreviewHandler::HandleReportUiEvent(const ListValue* args) {
  int event_group, event_number;
  if (!args->GetInteger(0, &event_group) || !args->GetInteger(1, &event_number))
    return;

  enum UiBucketGroups ui_bucket_group =
      static_cast<enum UiBucketGroups>(event_group);
  if (ui_bucket_group >= UI_BUCKET_GROUP_BOUNDARY)
    return;

  switch (ui_bucket_group) {
    case DESTINATION_SEARCH: {
      enum PrintDestinationBuckets event =
          static_cast<enum PrintDestinationBuckets>(event_number);
      if (event >= PRINT_DESTINATION_BUCKET_BOUNDARY)
        return;
      ReportPrintDestinationHistogram(event);
      break;
    }
    case GCP_PROMO: {
      enum GcpPromoBuckets event =
          static_cast<enum GcpPromoBuckets>(event_number);
      if (event >= GCP_PROMO_BUCKET_BOUNDARY)
        return;
      ReportGcpPromoHistogram(event);
      break;
    }
    default:
      break;
  }
}

void PrintPreviewHandler::HandleForceOpenNewTab(const ListValue* args) {
  std::string url;
  if (!args->GetString(0, &url))
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(GetInitiatorTab());
  if (!browser)
    return;
  chrome::AddSelectedTabWithURL(browser,
                                GURL(url),
                                content::PAGE_TRANSITION_LINK);
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
  initial_settings.SetString(printing::kSettingPrinterName, default_printer);
  initial_settings.SetBoolean(kDocumentHasSelection,
                              print_preview_ui->source_has_selection());
  initial_settings.SetBoolean(printing::kSettingShouldPrintSelectionOnly,
                              print_preview_ui->print_selection_only());
  printing::StickySettings* sticky_settings = GetStickySettings();
  sticky_settings->RestoreFromPrefs(Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext())->GetPrefs());
  if (sticky_settings->printer_app_state())
    initial_settings.SetString(kAppState,
                               *sticky_settings->printer_app_state());

  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  initial_settings.SetBoolean(kPrintAutomaticallyInKioskMode,
                              cmdline->HasSwitch(switches::kKioskModePrinting));

  if (print_preview_ui->source_is_modifiable())
    GetNumberFormatAndMeasurementSystem(&initial_settings);
  web_ui()->CallJavascriptFunction("setInitialSettings", initial_settings);
}

void PrintPreviewHandler::ClosePreviewDialogAndActivateInitiatorTab() {
  // Need to get the initiator tab before closing the print preview dialog.
  WebContents* initiator_tab = GetInitiatorTab();
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());

  print_preview_ui->OnClosePrintPreviewDialog();
  if (initiator_tab)
    initiator_tab->GetDelegate()->ActivateContents(initiator_tab);
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const DictionaryValue& settings_info) {
  VLOG(1) << "Get printer capabilities finished";
  web_ui()->CallJavascriptFunction("updateWithPrinterCapabilities",
                                   settings_info);
}

void PrintPreviewHandler::SendFailedToGetPrinterCapabilities(
    const std::string& printer_name) {
  VLOG(1) << "Get printer capabilities failed";
  base::StringValue printer_name_value(printer_name);
  web_ui()->CallJavascriptFunction("failedToGetPrinterCapabilities",
                                   printer_name_value);
}

void PrintPreviewHandler::SetupPrinterList(const ListValue& printers) {
  web_ui()->CallJavascriptFunction("setPrinters", printers);
}

void PrintPreviewHandler::SendCloudPrintEnabled() {
  Profile* profile = Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(prefs::kCloudPrintSubmitEnabled)) {
    GURL gcp_url(CloudPrintURL(profile).GetCloudPrintServiceURL());
    base::StringValue gcp_url_value(gcp_url.spec());
    web_ui()->CallJavascriptFunction("setUseCloudPrint", gcp_url_value);
  }
}

void PrintPreviewHandler::SendCloudPrintJob(const base::RefCountedBytes* data) {
  // BASE64 encode the job data.
  std::string raw_data(reinterpret_cast<const char*>(data->front()),
                       data->size());
  std::string base64_data;
  if (!base::Base64Encode(raw_data, &base64_data)) {
    NOTREACHED() << "Base64 encoding PDF data.";
  }
  StringValue data_value(base64_data);

  web_ui()->CallJavascriptFunction("printToCloud", data_value);
}

WebContents* PrintPreviewHandler::GetInitiatorTab() const {
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return NULL;
  return dialog_controller->GetInitiatorTab(preview_web_contents());
}

void PrintPreviewHandler::OnPrintDialogShown() {
  ClosePreviewDialogAndActivateInitiatorTab();
}

void PrintPreviewHandler::SelectFile(const base::FilePath& default_filename) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pdf"));
  file_type_info.support_drive = true;

  // Initializing save_path_ if it is not already initialized.
  printing::StickySettings* sticky_settings = GetStickySettings();
  if (!sticky_settings->save_path()) {
    // Allowing IO operation temporarily. It is ok to do so here because
    // the select file dialog performs IO anyway in order to display the
    // folders and also it is modal.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath file_path;
    PathService::Get(chrome::DIR_USER_DOCUMENTS, &file_path);
    sticky_settings->StoreSavePath(file_path);
    sticky_settings->SaveInPrefs(Profile::FromBrowserContext(
        preview_web_contents()->GetBrowserContext())->GetPrefs());
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(preview_web_contents())),
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      string16(),
      sticky_settings->save_path()->Append(default_filename),
      &file_type_info,
      0,
      FILE_PATH_LITERAL(""),
      platform_util::GetTopLevel(preview_web_contents()->GetNativeView()),
      NULL);
}

void PrintPreviewHandler::OnPrintPreviewDialogDestroyed() {
  WebContents* initiator_tab = GetInitiatorTab();
  if (!initiator_tab)
    return;

  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->set_observer(NULL);
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

void PrintPreviewHandler::FileSelected(const base::FilePath& path,
                                       int index, void* params) {
  // Updating |save_path_| to the newly selected folder.
  printing::StickySettings* sticky_settings = GetStickySettings();
  sticky_settings->StoreSavePath(path.DirName());
  sticky_settings->SaveInPrefs(Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext())->GetPrefs());
  web_ui()->CallJavascriptFunction("fileSelectionCompleted");
  print_to_pdf_path_.reset(new base::FilePath(path));
  PostPrintToPdfTask();
}

void PrintPreviewHandler::PostPrintToPdfTask() {
  scoped_refptr<base::RefCountedBytes> data;
  string16 title;
  if (!GetPreviewDataAndTitle(&data, &title)) {
    NOTREACHED() << "Preview data was checked before file dialog.";
    return;
  }
  printing::PreviewMetafile* metafile = new printing::PreviewMetafile;
  metafile->InitFromData(static_cast<const void*>(data->front()), data->size());
  // PrintToPdfCallback takes ownership of |metafile|.
#ifdef OS_CHROMEOS
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  drive::util::PrepareWritableFileAndRun(
      Profile::FromBrowserContext(preview_web_contents()->GetBrowserContext()),
      *print_to_pdf_path_,
      base::Bind(&PrintToPdfCallbackWithCheck, metafile));
#else
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&PrintToPdfCallback, metafile,
                                     *print_to_pdf_path_));
#endif

  print_to_pdf_path_.reset();
  ClosePreviewDialogAndActivateInitiatorTab();
}

void PrintPreviewHandler::FileSelectionCanceled(void* params) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  print_preview_ui->OnFileSelectionCancelled();
}

void PrintPreviewHandler::ClearInitiatorTabDetails() {
  WebContents* initiator_tab = GetInitiatorTab();
  if (!initiator_tab)
    return;

  // We no longer require the initiator tab details. Remove those details
  // associated with the preview dialog to allow the initiator tab to create
  // another preview dialog.
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (dialog_controller)
    dialog_controller->EraseInitiatorTabInfo(preview_web_contents());
}

bool PrintPreviewHandler::GetPreviewDataAndTitle(
    scoped_refptr<base::RefCountedBytes>* data,
    string16* title) const {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  scoped_refptr<base::RefCountedBytes> tmp_data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &tmp_data);

  if (!tmp_data) {
    // Nothing to print, no preview available.
    return false;
  }
  DCHECK(tmp_data->size() && tmp_data->front());

  *data = tmp_data;
  *title = print_preview_ui->initiator_tab_title();
  return true;
}

