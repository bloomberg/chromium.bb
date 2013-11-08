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
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/print_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/print_settings.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

namespace {

enum UserActionBuckets {
  PRINT_TO_PRINTER,
  PRINT_TO_PDF,
  CANCEL,
  FALLBACK_TO_ADVANCED_SETTINGS_DIALOG,
  PREVIEW_FAILED,
  PREVIEW_STARTED,
  INITIATOR_CRASHED,  // UNUSED
  INITIATOR_CLOSED,
  PRINT_WITH_CLOUD_PRINT,
  USERACTION_BUCKET_BOUNDARY
};

enum PrintSettingsBuckets {
  LANDSCAPE = 0,
  PORTRAIT,
  COLOR,
  BLACK_AND_WHITE,
  COLLATE,
  SIMPLEX,
  DUPLEX,
  TOTAL,
  HEADERS_AND_FOOTERS,
  CSS_BACKGROUND,
  SELECTION_ONLY,
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
// Name of a dictionary field holding the initiator title.
const char kInitiatorTitle[] = "initiatorTitle";
// Name of a dictionary field holding the measurement system according to the
// locale.
const char kMeasurementSystem[] = "measurementSystem";
// Name of a dictionary field holding the number format according to the locale.
const char kNumberFormat[] = "numberFormat";
// Name of a dictionary field specifying whether to print automatically in
// kiosk mode. See http://crbug.com/31395.
const char kPrintAutomaticallyInKioskMode[] = "printAutomaticallyInKioskMode";
#if defined(OS_WIN)
const char kHidePrintWithSystemDialogLink[] = "hidePrintWithSystemDialogLink";
#endif
// Name of a dictionary field holding the state of selection for document.
const char kDocumentHasSelection[] = "documentHasSelection";

// Additional printer capability setting keys.
const char kPrinterId[] = "printerId";
const char kDisableColorOption[] = "disableColorOption";
const char kSetDuplexAsDefault[] = "setDuplexAsDefault";
const char kPrinterDefaultDuplexValue[] = "printerDefaultDuplexValue";
#if defined(USE_CUPS)
const char kCUPSsColorModel[] = "cupsColorModel";
const char kCUPSsBWModel[] = "cupsBWModel";
#endif

#if defined(ENABLE_MDNS)
const int kPrivetPrinterSearchDurationSeconds = 3;
#endif

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

// Track the popularity of print settings and report the stats.
void ReportPrintSettingsStats(const DictionaryValue& settings) {
  ReportPrintSettingHistogram(TOTAL);

  bool landscape = false;
  if (settings.GetBoolean(printing::kSettingLandscape, &landscape))
    ReportPrintSettingHistogram(landscape ? LANDSCAPE : PORTRAIT);

  bool collate = false;
  if (settings.GetBoolean(printing::kSettingCollate, &collate) && collate)
    ReportPrintSettingHistogram(COLLATE);

  int duplex_mode = 0;
  if (settings.GetInteger(printing::kSettingDuplexMode, &duplex_mode))
    ReportPrintSettingHistogram(duplex_mode ? DUPLEX : SIMPLEX);

  int color_mode = 0;
  if (settings.GetInteger(printing::kSettingColor, &color_mode)) {
    ReportPrintSettingHistogram(
        printing::IsColorModelSelected(color_mode) ? COLOR : BLACK_AND_WHITE);
  }

  bool headers = false;
  if (settings.GetBoolean(printing::kSettingHeaderFooterEnabled, &headers) &&
      headers) {
    ReportPrintSettingHistogram(HEADERS_AND_FOOTERS);
  }

  bool css_background = false;
  if (settings.GetBoolean(printing::kSettingShouldPrintBackgrounds,
                          &css_background) && css_background) {
    ReportPrintSettingHistogram(CSS_BACKGROUND);
  }

  bool selection_only = false;
  if (settings.GetBoolean(printing::kSettingShouldPrintSelectionOnly,
                          &selection_only) && selection_only) {
    ReportPrintSettingHistogram(SELECTION_ONLY);
  }
}

// Callback that stores a PDF file on disk.
void PrintToPdfCallback(printing::Metafile* metafile,
                        const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  metafile->SaveTo(path);
  // |metafile| must be deleted on the UI thread.
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, metafile);
}

std::string GetDefaultPrinterOnFileThread(
    scoped_refptr<printing::PrintBackend> print_backend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string default_printer = print_backend->GetDefaultPrinterName();
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

void EnumeratePrintersOnFileThread(
    scoped_refptr<printing::PrintBackend> print_backend,
    base::ListValue* printers) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  VLOG(1) << "Enumerate printers start";
  printing::PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);

  for (printing::PrinterList::iterator it = printer_list.begin();
       it != printer_list.end(); ++it) {
    base::DictionaryValue* printer_info = new base::DictionaryValue;
    std::string printer_name;
#if defined(OS_MACOSX)
    // On Mac, |it->printer_description| specifies the printer name and
    // |it->printer_name| specifies the device name / printer queue name.
    printer_name = it->printer_description;
#else
    printer_name = it->printer_name;
#endif
    printer_info->SetString(printing::kSettingPrinterName, printer_name);
    printer_info->SetString(printing::kSettingDeviceName, it->printer_name);
    VLOG(1) << "Found printer " << printer_name
            << " with device name " << it->printer_name;
    printers->Append(printer_info);
  }
  VLOG(1) << "Enumerate printers finished, found " << printers->GetSize()
          << " printers";
}

typedef base::Callback<void(const base::DictionaryValue*)>
    GetPrinterCapabilitiesSuccessCallback;
typedef base::Callback<void(const std::string&)>
    GetPrinterCapabilitiesFailureCallback;

void GetPrinterCapabilitiesOnFileThread(
    scoped_refptr<printing::PrintBackend> print_backend,
    const std::string& printer_name,
    const GetPrinterCapabilitiesSuccessCallback& success_cb,
    const GetPrinterCapabilitiesFailureCallback& failure_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!printer_name.empty());

  VLOG(1) << "Get printer capabilities start for " << printer_name;
  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (!print_backend->IsValidPrinter(printer_name)) {
    // TODO(gene): Notify explicitly if printer is not valid, instead of
    // failed to get capabilities.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(failure_cb, printer_name));
    return;
  }

  printing::PrinterSemanticCapsAndDefaults info;
  if (!print_backend->GetPrinterSemanticCapsAndDefaults(printer_name, &info)) {
    LOG(WARNING) << "Failed to get capabilities for " << printer_name;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(failure_cb, printer_name));
    return;
  }

  scoped_ptr<base::DictionaryValue> settings_info(new base::DictionaryValue);
  settings_info->SetString(kPrinterId, printer_name);
  settings_info->SetBoolean(kDisableColorOption, !info.color_changeable);
  settings_info->SetBoolean(printing::kSettingSetColorAsDefault,
                            info.color_default);
#if defined(USE_CUPS)
  settings_info->SetInteger(kCUPSsColorModel, info.color_model);
  settings_info->SetInteger(kCUPSsBWModel, info.bw_model);
#endif

  // TODO(gene): Make new capabilities format for Print Preview
  // that will suit semantic capabiltities better.
  // Refactor pld API code below
  bool default_duplex = info.duplex_capable ?
      (info.duplex_default != printing::SIMPLEX) : false;
  int duplex_value = info.duplex_capable ?
      printing::LONG_EDGE : printing::UNKNOWN_DUPLEX_MODE;
  settings_info->SetBoolean(kSetDuplexAsDefault, default_duplex);
  settings_info->SetInteger(kPrinterDefaultDuplexValue, duplex_value);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(success_cb, base::Owned(settings_info.release())));
}

base::LazyInstance<printing::StickySettings> g_sticky_settings =
    LAZY_INSTANCE_INITIALIZER;

printing::StickySettings* GetStickySettings() {
  return g_sticky_settings.Pointer();
}

}  // namespace

#if defined(USE_CUPS)
struct PrintPreviewHandler::CUPSPrinterColorModels {
  std::string printer_name;
  printing::ColorModel color_model;
  printing::ColorModel bw_model;
};
#endif

class PrintPreviewHandler::AccessTokenService
    : public OAuth2TokenService::Consumer {
 public:
  explicit AccessTokenService(PrintPreviewHandler* handler)
      : handler_(handler),
        weak_factory_(this) {
  }

  void RequestToken(const std::string& type) {
    if (requests_.find(type) != requests_.end())
      return;  // Already in progress.

    OAuth2TokenService* service = NULL;
    std::string account_id;
    if (type == "profile") {
      Profile* profile = Profile::FromWebUI(handler_->web_ui());
      if (profile) {
        ProfileOAuth2TokenService* token_service =
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
        account_id = token_service->GetPrimaryAccountId();
        service = token_service;
      }
    } else if (type == "device") {
#if defined(OS_CHROMEOS)
      chromeos::DeviceOAuth2TokenServiceFactory::Get(
          base::Bind(
              &AccessTokenService::DidGetTokenService,
              weak_factory_.GetWeakPtr(),
              type));
      return;
#endif
    }

    ContinueRequestToken(type, service, account_id);
  }

#if defined(OS_CHROMEOS)
  // Continuation of RequestToken().
  void DidGetTokenService(const std::string& type,
                          chromeos::DeviceOAuth2TokenService* token_service) {
    std::string account_id;
    if (token_service)
      account_id = token_service->GetRobotAccountId();
    ContinueRequestToken(type,
                         token_service,
                         account_id);
  }
#endif

  // Continuation of RequestToken().
  void ContinueRequestToken(const std::string& type,
                            OAuth2TokenService* service,
                            const std::string& account_id) {
    if (service) {
      OAuth2TokenService::ScopeSet oauth_scopes;
      oauth_scopes.insert(cloud_print::kCloudPrintAuth);
      scoped_ptr<OAuth2TokenService::Request> request(
          service->StartRequest(account_id, oauth_scopes, this));
      requests_[type].reset(request.release());
    } else {
      handler_->SendAccessToken(type, std::string());  // Unknown type.
    }
  }

  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE {
    OnServiceResponce(request, access_token);
  }

  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE {
    OnServiceResponce(request, std::string());
  }

 private:
  void OnServiceResponce(const OAuth2TokenService::Request* request,
                         const std::string& access_token) {
    for (Requests::iterator i = requests_.begin(); i != requests_.end(); ++i) {
      if (i->second == request) {
        handler_->SendAccessToken(i->first, access_token);
        requests_.erase(i);
        return;
      }
    }
    NOTREACHED();
  }

  typedef std::map<std::string,
                   linked_ptr<OAuth2TokenService::Request> > Requests;
  Requests requests_;
  PrintPreviewHandler* handler_;
  base::WeakPtrFactory<AccessTokenService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenService);
};

PrintPreviewHandler::PrintPreviewHandler()
    : print_backend_(printing::PrintBackend::CreateInstance(NULL)),
      regenerate_preview_request_count_(0),
      manage_printers_dialog_request_count_(0),
      manage_cloud_printers_dialog_request_count_(0),
      reported_failed_preview_(false),
      has_logged_printers_count_(false),
      weak_factory_(this) {
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
  web_ui()->RegisterMessageCallback("getAccessToken",
      base::Bind(&PrintPreviewHandler::HandleGetAccessToken,
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
  web_ui()->RegisterMessageCallback("printWithCloudPrintDialog",
      base::Bind(&PrintPreviewHandler::HandlePrintWithCloudPrintDialog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("forceOpenNewTab",
      base::Bind(&PrintPreviewHandler::HandleForceOpenNewTab,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getPrivetPrinters",
      base::Bind(&PrintPreviewHandler::HandleGetPrivetPrinters,
                 base::Unretained(this)));
}

bool PrintPreviewHandler::PrivetPrintingEnabled() {
#if defined(ENABLE_MDNS)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisableDeviceDiscovery) &&
      command_line->HasSwitch(switches::kEnablePrivetLocalPrinting);
#else
  return false;
#endif
}

WebContents* PrintPreviewHandler::preview_web_contents() const {
  return web_ui()->GetWebContents();
}

void PrintPreviewHandler::HandleGetPrinters(const ListValue* /*args*/) {
  base::ListValue* results = new base::ListValue;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&EnumeratePrintersOnFileThread, print_backend_,
                 base::Unretained(results)),
      base::Bind(&PrintPreviewHandler::SetupPrinterList,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(results)));
}

void PrintPreviewHandler::HandleGetPrivetPrinters(const base::ListValue* args) {
#if defined(ENABLE_MDNS)
  if (PrivetPrintingEnabled()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    service_discovery_client_ =
        local_discovery::ServiceDiscoverySharedClient::GetInstance();
    printer_lister_.reset(new local_discovery::PrivetLocalPrinterLister(
        service_discovery_client_.get(),
        profile->GetRequestContext(),
        this));
    printer_lister_->Start();

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrintPreviewHandler::StopPrivetPrinterSearch,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kPrivetPrinterSearchDurationSeconds));
  }
#endif

  if (!PrivetPrintingEnabled()) {
    web_ui()->CallJavascriptFunction("onPrivetPrinterSearchDone");
  }
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

  WebContents* initiator = GetInitiator();
  if (!initiator) {
    ReportUserActionHistogram(INITIATOR_CLOSED);
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
                        initiator->GetTitle());
    std::string url;
    content::NavigationEntry* entry =
        initiator->GetController().GetActiveEntry();
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
  RenderViewHost* rvh = initiator->GetRenderViewHost();
  rvh->Send(new PrintMsg_PrintPreview(rvh->GetRoutingID(), *settings));
}

void PrintPreviewHandler::HandlePrint(const ListValue* args) {
  ReportStats();

  // Record the number of times the user requests to regenerate preview data
  // before printing.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforePrint",
                       regenerate_preview_request_count_);

  WebContents* initiator = GetInitiator();
  if (initiator) {
    RenderViewHost* rvh = initiator->GetRenderViewHost();
    rvh->Send(new PrintMsg_ResetScriptedPrintCount(rvh->GetRoutingID()));
  }

  scoped_ptr<DictionaryValue> settings(GetSettingsDictionary(args));
  if (!settings.get())
    return;

  // Never try to add headers/footers here. It's already in the generated PDF.
  settings->SetBoolean(printing::kSettingHeaderFooterEnabled, false);

  bool print_to_pdf = false;
  bool is_cloud_printer = false;

  bool open_pdf_in_preview = false;
#if defined(OS_MACOSX)
  open_pdf_in_preview = settings->HasKey(printing::kSettingOpenPDFInPreview);
#endif

  if (!open_pdf_in_preview) {
    settings->GetBoolean(printing::kSettingPrintToPDF, &print_to_pdf);
    is_cloud_printer = settings->HasKey(printing::kSettingCloudPrintId);
  }

  int page_count = 0;
  settings->GetInteger(printing::kSettingPreviewPageCount, &page_count);

  if (print_to_pdf) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPDF", page_count);
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
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToCloudPrint",
                         page_count);
    ReportUserActionHistogram(PRINT_WITH_CLOUD_PRINT);
    SendCloudPrintJob(data.get());
  } else {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPrinter", page_count);
    ReportUserActionHistogram(PRINT_TO_PRINTER);
    ReportPrintSettingsStats(*settings);

    // This tries to activate the initiator as well, so do not clear the
    // association with the initiator yet.
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        web_ui()->GetController());
    print_preview_ui->OnHidePreviewDialog();

    // Do this so the initiator can open a new print preview dialog, while the
    // current print preview dialog is still handling its print job.
    ClearInitiatorDetails();

    // The PDF being printed contains only the pages that the user selected,
    // so ignore the page range and print all pages.
    settings->Remove(printing::kSettingPageRange, NULL);
    // Reset selection only flag for the same reason.
    settings->SetBoolean(printing::kSettingShouldPrintSelectionOnly, false);

#if defined(USE_CUPS)
    if (!open_pdf_in_preview)  // We can get here even for cloud printers.
      ConvertColorSettingToCUPSColorModel(settings.get());
#endif

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
    if (initiator) {
      printing::PrintViewManager* print_view_manager =
          printing::PrintViewManager::FromWebContents(initiator);
      print_view_manager->PrintPreviewDone();
    }
  }
}

void PrintPreviewHandler::PrintToPdf() {
  if (!print_to_pdf_path_.empty()) {
    // User has already selected a path, no need to show the dialog again.
    PostPrintToPdfTask();
  } else if (!select_file_dialog_.get() ||
             !select_file_dialog_->IsRunning(platform_util::GetTopLevel(
                 preview_web_contents()->GetView()->GetNativeView()))) {
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        web_ui()->GetController());
    // Pre-populating select file dialog with print job title.
    string16 print_job_title_utf16 = print_preview_ui->initiator_title();

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
  WebContents* initiator = GetInitiator();
  if (initiator)
    ClearInitiatorDetails();
  gfx::NativeWindow parent = initiator ?
      initiator->GetView()->GetTopLevelNativeWindow() :
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

  GetPrinterCapabilitiesSuccessCallback success_cb =
      base::Bind(&PrintPreviewHandler::SendPrinterCapabilities,
                 weak_factory_.GetWeakPtr());
  GetPrinterCapabilitiesFailureCallback failure_cb =
      base::Bind(&PrintPreviewHandler::SendFailedToGetPrinterCapabilities,
                 weak_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GetPrinterCapabilitiesOnFileThread,
                 print_backend_, printer_name, success_cb, failure_cb));
}

void PrintPreviewHandler::OnSigninComplete() {
  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(web_ui()->GetController());
  if (print_preview_ui)
    print_preview_ui->OnReloadPrintersList();
}

void PrintPreviewHandler::HandleSignin(const ListValue* /*args*/) {
  gfx::NativeWindow modal_parent = platform_util::GetTopLevel(
      preview_web_contents()->GetView()->GetNativeView());
  print_dialog_cloud::CreateCloudPrintSigninDialog(
      preview_web_contents()->GetBrowserContext(),
      modal_parent,
      base::Bind(&PrintPreviewHandler::OnSigninComplete,
                 weak_factory_.GetWeakPtr()));
}

void PrintPreviewHandler::HandleGetAccessToken(const base::ListValue* args) {
  std::string type;
  if (!args->GetString(0, &type))
    return;
  if (!token_service_)
    token_service_.reset(new AccessTokenService(this));
  token_service_->RequestToken(type);
}

void PrintPreviewHandler::PrintWithCloudPrintDialog() {
  // Record the number of times the user asks to print via cloud print
  // instead of the print preview dialog.
  ReportStats();

  scoped_refptr<base::RefCountedBytes> data;
  string16 title;
  if (!GetPreviewDataAndTitle(&data, &title)) {
    // Nothing to print, no preview available.
    return;
  }

  gfx::NativeWindow modal_parent = platform_util::GetTopLevel(
      preview_web_contents()->GetView()->GetNativeView());
  print_dialog_cloud::CreatePrintDialogForBytes(
      preview_web_contents()->GetBrowserContext(),
      modal_parent,
      data.get(),
      title,
      string16(),
      std::string("application/pdf"));

  // Once the cloud print dialog comes up we're no longer in a background
  // printing situation.  Close the print preview.
  // TODO(abodenha@chromium.org) The flow should be changed as described in
  // http://code.google.com/p/chromium/issues/detail?id=44093
  ClosePreviewDialog();
}

void PrintPreviewHandler::HandleManageCloudPrint(const ListValue* /*args*/) {
  ++manage_cloud_printers_dialog_request_count_;
  Profile* profile = Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  preview_web_contents()->OpenURL(
      content::OpenURLParams(
          CloudPrintURL(profile).GetCloudPrintServiceManageURL(),
          content::Referrer(),
          NEW_FOREGROUND_TAB,
          content::PAGE_TRANSITION_LINK,
          false));
}

void PrintPreviewHandler::HandleShowSystemDialog(const ListValue* /*args*/) {
  ReportStats();
  ReportUserActionHistogram(FALLBACK_TO_ADVANCED_SETTINGS_DIALOG);

  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator);
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

void PrintPreviewHandler::HandlePrintWithCloudPrintDialog(
    const base::ListValue* args) {
  int page_count = 0;
  if (!args || !args->GetInteger(0, &page_count))
    return;
  UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToCloudPrintWebDialog",
                       page_count);

  PrintWithCloudPrintDialog();
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
  // Send before SendInitialSettings to allow cloud printer auto select.
  SendCloudPrintEnabled();
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GetDefaultPrinterOnFileThread, print_backend_),
      base::Bind(&PrintPreviewHandler::SendInitialSettings,
                 weak_factory_.GetWeakPtr()));
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
  Browser* browser = chrome::FindBrowserWithWebContents(GetInitiator());
  if (!browser)
    return;
  chrome::AddSelectedTabWithURL(browser,
                                GURL(url),
                                content::PAGE_TRANSITION_LINK);
}

void PrintPreviewHandler::SendInitialSettings(
    const std::string& default_printer) {
  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(web_ui()->GetController());

  base::DictionaryValue initial_settings;
  initial_settings.SetString(kInitiatorTitle,
                             print_preview_ui->initiator_title());
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
#if defined(OS_WIN)
  // In Win8 metro, the system print dialog can only open on the desktop.  Doing
  // so will cause the browser to appear hung, so we don't show the link in
  // metro.
  bool is_ash = (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH);
  initial_settings.SetBoolean(kHidePrintWithSystemDialogLink, is_ash);
#endif

  if (print_preview_ui->source_is_modifiable())
    GetNumberFormatAndMeasurementSystem(&initial_settings);
  web_ui()->CallJavascriptFunction("setInitialSettings", initial_settings);
}

void PrintPreviewHandler::ClosePreviewDialog() {
  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(web_ui()->GetController());
  print_preview_ui->OnClosePrintPreviewDialog();
}

void PrintPreviewHandler::SendAccessToken(const std::string& type,
                                          const std::string& access_token) {
  VLOG(1) << "Get getAccessToken finished";
  web_ui()->CallJavascriptFunction("onDidGetAccessToken", StringValue(type),
                                   StringValue(access_token));
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const DictionaryValue* settings_info) {
  VLOG(1) << "Get printer capabilities finished";

#if defined(USE_CUPS)
  SaveCUPSColorSetting(settings_info);
#endif

  web_ui()->CallJavascriptFunction("updateWithPrinterCapabilities",
                                   *settings_info);
}

void PrintPreviewHandler::SendFailedToGetPrinterCapabilities(
    const std::string& printer_name) {
  VLOG(1) << "Get printer capabilities failed";
  base::StringValue printer_name_value(printer_name);
  web_ui()->CallJavascriptFunction("failedToGetPrinterCapabilities",
                                   printer_name_value);
}

void PrintPreviewHandler::SetupPrinterList(const base::ListValue* printers) {
  if (!has_logged_printers_count_) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.NumberOfPrinters", printers->GetSize());
    has_logged_printers_count_ = true;
  }

  web_ui()->CallJavascriptFunction("setPrinters", *printers);
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

WebContents* PrintPreviewHandler::GetInitiator() const {
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return NULL;
  return dialog_controller->GetInitiator(preview_web_contents());
}

void PrintPreviewHandler::OnPrintDialogShown() {
  ClosePreviewDialog();
}

void PrintPreviewHandler::SelectFile(const base::FilePath& default_filename) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pdf"));

  // Initializing |save_path_| if it is not already initialized.
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
      base::FilePath::StringType(),
      platform_util::GetTopLevel(
          preview_web_contents()->GetView()->GetNativeView()),
      NULL);
}

void PrintPreviewHandler::OnPrintPreviewDialogDestroyed() {
  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator);
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
  print_to_pdf_path_ = path;
  PostPrintToPdfTask();
}

void PrintPreviewHandler::PostPrintToPdfTask() {
  scoped_refptr<base::RefCountedBytes> data;
  string16 title;
  if (!GetPreviewDataAndTitle(&data, &title)) {
    NOTREACHED() << "Preview data was checked before file dialog.";
    return;
  }
  scoped_ptr<printing::PreviewMetafile> metafile(new printing::PreviewMetafile);
  metafile->InitFromData(static_cast<const void*>(data->front()), data->size());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PrintToPdfCallback, metafile.release(), print_to_pdf_path_));
  print_to_pdf_path_ = base::FilePath();
  ClosePreviewDialog();
}

void PrintPreviewHandler::FileSelectionCanceled(void* params) {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  print_preview_ui->OnFileSelectionCancelled();
}

void PrintPreviewHandler::ClearInitiatorDetails() {
  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  // We no longer require the initiator details. Remove those details associated
  // with the preview dialog to allow the initiator to create another preview
  // dialog.
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (dialog_controller)
    dialog_controller->EraseInitiatorInfo(preview_web_contents());
}

bool PrintPreviewHandler::GetPreviewDataAndTitle(
    scoped_refptr<base::RefCountedBytes>* data,
    string16* title) const {
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      web_ui()->GetController());
  scoped_refptr<base::RefCountedBytes> tmp_data;
  print_preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &tmp_data);

  if (!tmp_data.get()) {
    // Nothing to print, no preview available.
    return false;
  }
  DCHECK(tmp_data->size() && tmp_data->front());

  *data = tmp_data;
  *title = print_preview_ui->initiator_title();
  return true;
}

#if defined(USE_CUPS)
void PrintPreviewHandler::SaveCUPSColorSetting(
    const base::DictionaryValue* settings) {
  cups_printer_color_models_.reset(new CUPSPrinterColorModels);
  settings->GetString(kPrinterId, &cups_printer_color_models_->printer_name);
  settings->GetInteger(
      kCUPSsColorModel,
      reinterpret_cast<int*>(&cups_printer_color_models_->color_model));
  settings->GetInteger(
      kCUPSsBWModel,
      reinterpret_cast<int*>(&cups_printer_color_models_->bw_model));
}

void PrintPreviewHandler::ConvertColorSettingToCUPSColorModel(
    base::DictionaryValue* settings) const {
  if (!cups_printer_color_models_)
    return;

  // Sanity check the printer name.
  std::string printer_name;
  if (!settings->GetString(printing::kSettingDeviceName, &printer_name) ||
      printer_name != cups_printer_color_models_->printer_name) {
    NOTREACHED();
    return;
  }

  int color;
  if (!settings->GetInteger(printing::kSettingColor, &color)) {
    NOTREACHED();
    return;
  }

  if (color == printing::GRAY) {
    if (cups_printer_color_models_->bw_model != printing::UNKNOWN_COLOR_MODEL) {
      settings->SetInteger(printing::kSettingColor,
                           cups_printer_color_models_->bw_model);
    }
    return;
  }

  printing::ColorModel color_model = cups_printer_color_models_->color_model;
  if (color_model != printing::UNKNOWN_COLOR_MODEL)
    settings->SetInteger(printing::kSettingColor, color_model);
}

#endif


#if defined(ENABLE_MDNS)
void PrintPreviewHandler::LocalPrinterChanged(
    bool added,
    const std::string& name,
    const local_discovery::DeviceDescription& description) {
  base::DictionaryValue info;
  info.SetString("serviceName", name);
  info.SetString("name", description.name);

  web_ui()->CallJavascriptFunction("onPrivetPrinterChanged", info);
}

void PrintPreviewHandler::LocalPrinterRemoved(const std::string& name) {
}

void PrintPreviewHandler::LocalPrinterCacheFlushed() {
}

void PrintPreviewHandler::StopPrivetPrinterSearch() {
  printer_lister_.reset();
  service_discovery_client_ = NULL;
  web_ui()->CallJavascriptFunction("onPrivetPrinterSearchDone");
}

#endif
