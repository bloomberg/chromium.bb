// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <ctype.h>
#include <stddef.h>
#include <map>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_cdd_conversion.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/cloud_devices/common/printer_description.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/print_messages.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/url_util.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/metafile.h"
#include "printing/pdf_metafile_skia.h"
#include "printing/pdf_render_settings.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"
#include "printing/units.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#endif

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/printing/cloud_print/privet_constants.h"
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
  PRINT_WITH_PRIVET,
  PRINT_WITH_EXTENSION,
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
  EXTERNAL_PDF_PREVIEW,
  PAGE_RANGE,
  DEFAULT_MEDIA,
  NON_DEFAULT_MEDIA,
  COPIES,
  NON_DEFAULT_MARGINS,
  DISTILL_PAGE,
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
// Dictionary field to indicate whether Chrome is running in forced app (app
// kiosk) mode. It's not the same as desktop Chrome kiosk (the one above).
const char kAppKioskMode[] = "appKioskMode";
// Dictionary field to store Cloud Print base URL.
const char kCloudPrintUrl[] = "cloudPrintUrl";
#if defined(OS_WIN)
const char kHidePrintWithSystemDialogLink[] = "hidePrintWithSystemDialogLink";
#endif
// Name of a dictionary field holding the state of selection for document.
const char kDocumentHasSelection[] = "documentHasSelection";
// Dictionary field holding the default destination selection rules.
const char kDefaultDestinationSelectionRules[] =
    "defaultDestinationSelectionRules";

// Id of the predefined PDF printer.
const char kLocalPdfPrinterId[] = "Save as PDF";

// Additional printer capability setting keys.
const char kPrinterId[] = "printerId";
const char kPrinterCapabilities[] = "capabilities";

// Get the print job settings dictionary from |args|. The caller takes
// ownership of the returned DictionaryValue. Returns NULL on failure.
scoped_ptr<base::DictionaryValue> GetSettingsDictionary(
    const base::ListValue* args) {
  std::string json_str;
  if (!args->GetString(0, &json_str)) {
    NOTREACHED() << "Could not read JSON argument";
    return NULL;
  }
  if (json_str.empty()) {
    NOTREACHED() << "Empty print job settings";
    return NULL;
  }
  scoped_ptr<base::DictionaryValue> settings =
      base::DictionaryValue::From(base::JSONReader::Read(json_str));
  if (!settings) {
    NOTREACHED() << "Print job settings must be a dictionary.";
    return NULL;
  }

  if (settings->empty()) {
    NOTREACHED() << "Print job settings dictionary is empty";
    return NULL;
  }

  return settings;
}

// Track the popularity of print settings and report the stats.
void ReportPrintSettingsStats(const base::DictionaryValue& settings) {
  ReportPrintSettingHistogram(TOTAL);

  const base::ListValue* page_range_array = NULL;
  if (settings.GetList(printing::kSettingPageRange, &page_range_array) &&
      !page_range_array->empty()) {
    ReportPrintSettingHistogram(PAGE_RANGE);
  }

  const base::DictionaryValue* media_size_value = NULL;
  if (settings.GetDictionary(printing::kSettingMediaSize, &media_size_value) &&
      !media_size_value->empty()) {
    bool is_default = false;
    if (media_size_value->GetBoolean(printing::kSettingMediaSizeIsDefault,
                                     &is_default) &&
        is_default) {
      ReportPrintSettingHistogram(DEFAULT_MEDIA);
    } else {
      ReportPrintSettingHistogram(NON_DEFAULT_MEDIA);
    }
  }

  bool landscape = false;
  if (settings.GetBoolean(printing::kSettingLandscape, &landscape))
    ReportPrintSettingHistogram(landscape ? LANDSCAPE : PORTRAIT);

  int copies = 1;
  if (settings.GetInteger(printing::kSettingCopies, &copies) && copies > 1)
    ReportPrintSettingHistogram(COPIES);

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

  int margins_type = 0;
  if (settings.GetInteger(printing::kSettingMarginsType, &margins_type) &&
      margins_type != 0) {
    ReportPrintSettingHistogram(NON_DEFAULT_MARGINS);
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

  bool external_preview = false;
  if (settings.GetBoolean(printing::kSettingOpenPDFInPreview,
                          &external_preview) && external_preview) {
    ReportPrintSettingHistogram(EXTERNAL_PDF_PREVIEW);
  }

  bool distill_page = false;
  if (settings.GetBoolean(printing::kSettingDistillPageEnabled,
                          &distill_page) && distill_page) {
    ReportPrintSettingHistogram(DISTILL_PAGE);
  }
}

// Callback that stores a PDF file on disk.
void PrintToPdfCallback(const scoped_refptr<base::RefCountedBytes>& data,
                        const base::FilePath& path,
                        const base::Closure& pdf_file_saved_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  printing::PdfMetafileSkia metafile;
  metafile.InitFromData(static_cast<const void*>(data->front()), data->size());
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  metafile.SaveTo(&file);
  if (!pdf_file_saved_closure.is_null())
    pdf_file_saved_closure.Run();
}

std::string GetDefaultPrinterOnFileThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(NULL));

  std::string default_printer = print_backend->GetDefaultPrinterName();
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

class PrintingContextDelegate : public printing::PrintingContext::Delegate {
 public:
  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { return NULL; }
  std::string GetAppLocale() override {
    return g_browser_process->GetApplicationLocale();
  }
};

gfx::Size GetDefaultPdfMediaSizeMicrons() {
  PrintingContextDelegate delegate;
  scoped_ptr<printing::PrintingContext> printing_context(
      printing::PrintingContext::Create(&delegate));
  if (printing::PrintingContext::OK != printing_context->UsePdfSettings() ||
      printing_context->settings().device_units_per_inch() <= 0) {
    return gfx::Size();
  }
  gfx::Size pdf_media_size = printing_context->GetPdfPaperSizeDeviceUnits();
  float deviceMicronsPerDeviceUnit =
      (printing::kHundrethsMMPerInch * 10.0f) /
      printing_context->settings().device_units_per_inch();
  return gfx::Size(pdf_media_size.width() * deviceMicronsPerDeviceUnit,
                   pdf_media_size.height() * deviceMicronsPerDeviceUnit);
}

typedef base::Callback<void(const base::DictionaryValue*)>
    GetPdfCapabilitiesCallback;

scoped_ptr<base::DictionaryValue> GetPdfCapabilitiesOnFileThread(
    const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  cloud_devices::CloudDeviceDescription description;
  using namespace cloud_devices::printer;

  OrientationCapability orientation;
  orientation.AddOption(cloud_devices::printer::PORTRAIT);
  orientation.AddOption(cloud_devices::printer::LANDSCAPE);
  orientation.AddDefaultOption(AUTO_ORIENTATION, true);
  orientation.SaveTo(&description);

  ColorCapability color;
  {
    Color standard_color(STANDARD_COLOR);
    standard_color.vendor_id = base::IntToString(printing::COLOR);
    color.AddDefaultOption(standard_color, true);
  }
  color.SaveTo(&description);

  static const cloud_devices::printer::MediaType kPdfMedia[] = {
    ISO_A0,
    ISO_A1,
    ISO_A2,
    ISO_A3,
    ISO_A4,
    ISO_A5,
    NA_LEGAL,
    NA_LETTER,
    NA_LEDGER
  };
  const gfx::Size default_media_size = GetDefaultPdfMediaSizeMicrons();
  Media default_media(
      "", "", default_media_size.width(), default_media_size.height());
  if (!default_media.MatchBySize() ||
      std::find(kPdfMedia,
                kPdfMedia + arraysize(kPdfMedia),
                default_media.type) == kPdfMedia + arraysize(kPdfMedia)) {
    default_media = Media(locale == "en-US" ? NA_LETTER : ISO_A4);
  }
  MediaCapability media;
  for (size_t i = 0; i < arraysize(kPdfMedia); ++i) {
    Media media_option(kPdfMedia[i]);
    media.AddDefaultOption(media_option,
                           default_media.type == media_option.type);
  }
  media.SaveTo(&description);

  return scoped_ptr<base::DictionaryValue>(description.root().DeepCopy());
}

scoped_ptr<base::DictionaryValue> GetLocalPrinterCapabilitiesOnFileThread(
    const std::string& printer_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(NULL));

  VLOG(1) << "Get printer capabilities start for " << printer_name;
  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (!print_backend->IsValidPrinter(printer_name)) {
    LOG(WARNING) << "Invalid printer " << printer_name;
    return scoped_ptr<base::DictionaryValue>();
  }

  printing::PrinterSemanticCapsAndDefaults info;
  if (!print_backend->GetPrinterSemanticCapsAndDefaults(printer_name, &info)) {
    LOG(WARNING) << "Failed to get capabilities for " << printer_name;
    return scoped_ptr<base::DictionaryValue>();
  }

  scoped_ptr<base::DictionaryValue> description(
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(info));
  if (!description) {
    LOG(WARNING) << "Failed to convert capabilities for " << printer_name;
    return scoped_ptr<base::DictionaryValue>();
  }

  return description;
}

void EnumeratePrintersOnFileThread(base::ListValue* printers) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(NULL));

  VLOG(1) << "Enumerate printers start";
  printing::PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);

  for (printing::PrinterList::iterator it = printer_list.begin();
       it != printer_list.end(); ++it) {
    base::DictionaryValue* printer_info = new base::DictionaryValue;
    printers->Append(printer_info);
    std::string printer_name;
    std::string printer_description;
#if defined(OS_MACOSX)
    // On Mac, |it->printer_description| specifies the printer name and
    // |it->printer_name| specifies the device name / printer queue name.
    printer_name = it->printer_description;
    if (!it->options[kDriverNameTagName].empty())
      printer_description = it->options[kDriverNameTagName];
#else
    printer_name = it->printer_name;
    printer_description = it->printer_description;
#endif
    printer_info->SetString(printing::kSettingDeviceName, it->printer_name);
    printer_info->SetString(printing::kSettingPrinterDescription,
                            printer_description);
    printer_info->SetString(printing::kSettingPrinterName, printer_name);
    VLOG(1) << "Found printer " << printer_name
            << " with device name " << it->printer_name;

    base::DictionaryValue* options = new base::DictionaryValue;
    printer_info->Set(printing::kSettingPrinterOptions, options);
    for (std::map<std::string, std::string>::iterator opt = it->options.begin();
         opt != it->options.end();
         ++opt) {
      options->SetString(opt->first, opt->second);
    }

    VLOG(1) << "Found printer " << printer_name << " with device name "
            << it->printer_name;
  }
  VLOG(1) << "Enumerate printers finished, found " << printers->GetSize()
          << " printers";
}

typedef base::Callback<void(const base::DictionaryValue*)>
    GetPrinterCapabilitiesSuccessCallback;
typedef base::Callback<void(const std::string&)>
    GetPrinterCapabilitiesFailureCallback;

void GetPrinterCapabilitiesOnFileThread(
    const std::string& printer_name,
    const std::string& locale,
    const GetPrinterCapabilitiesSuccessCallback& success_cb,
    const GetPrinterCapabilitiesFailureCallback& failure_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!printer_name.empty());

  scoped_ptr<base::DictionaryValue> printer_capabilities(
      printer_name == kLocalPdfPrinterId ?
      GetPdfCapabilitiesOnFileThread(locale) :
      GetLocalPrinterCapabilitiesOnFileThread(printer_name));
  if (!printer_capabilities) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(failure_cb, printer_name));
    return;
  }

  scoped_ptr<base::DictionaryValue> printer_info(new base::DictionaryValue);
  printer_info->SetString(kPrinterId, printer_name);
  printer_info->Set(kPrinterCapabilities, printer_capabilities.release());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(success_cb, base::Owned(printer_info.release())));
}

base::LazyInstance<printing::StickySettings> g_sticky_settings =
    LAZY_INSTANCE_INITIALIZER;

printing::StickySettings* GetStickySettings() {
  return g_sticky_settings.Pointer();
}

// Returns a unique path for |path|, just like with downloads.
base::FilePath GetUniquePath(const base::FilePath& path) {
  base::FilePath unique_path = path;
  int uniquifier =
      base::GetUniquePathNumber(path, base::FilePath::StringType());
  if (uniquifier > 0) {
    unique_path = unique_path.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", uniquifier));
  }
  return unique_path;
}

}  // namespace

class PrintPreviewHandler::AccessTokenService
    : public OAuth2TokenService::Consumer {
 public:
  explicit AccessTokenService(PrintPreviewHandler* handler)
      : OAuth2TokenService::Consumer("print_preview"),
        handler_(handler) {
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
        SigninManagerBase* signin_manager =
            SigninManagerFactory::GetInstance()->GetForProfile(profile);
        account_id = signin_manager->GetAuthenticatedAccountId();
        service = token_service;
      }
    } else if (type == "device") {
#if defined(OS_CHROMEOS)
      chromeos::DeviceOAuth2TokenService* token_service =
          chromeos::DeviceOAuth2TokenServiceFactory::Get();
      account_id = token_service->GetRobotAccountId();
      service = token_service;
#endif
    }

    if (service) {
      OAuth2TokenService::ScopeSet oauth_scopes;
      oauth_scopes.insert(cloud_devices::kCloudPrintAuthScope);
      scoped_ptr<OAuth2TokenService::Request> request(
          service->StartRequest(account_id, oauth_scopes, this));
      requests_[type].reset(request.release());
    } else {
      handler_->SendAccessToken(type, std::string());  // Unknown type.
    }
  }

  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {
    OnServiceResponce(request, access_token);
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    OnServiceResponce(request, std::string());
  }

 private:
  void OnServiceResponce(const OAuth2TokenService::Request* request,
                         const std::string& access_token) {
    for (Requests::iterator i = requests_.begin(); i != requests_.end(); ++i) {
      if (i->second.get() == request) {
        handler_->SendAccessToken(i->first, access_token);
        requests_.erase(i);
        return;
      }
    }
    NOTREACHED();
  }

  using Requests =
      std::map<std::string, scoped_ptr<OAuth2TokenService::Request>>;
  Requests requests_;
  PrintPreviewHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenService);
};

PrintPreviewHandler::PrintPreviewHandler()
    : regenerate_preview_request_count_(0),
      manage_printers_dialog_request_count_(0),
      manage_cloud_printers_dialog_request_count_(0),
      reported_failed_preview_(false),
      has_logged_printers_count_(false),
      gaia_cookie_manager_service_(NULL),
      weak_factory_(this) {
  ReportUserActionHistogram(PREVIEW_STARTED);
}

PrintPreviewHandler::~PrintPreviewHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  UnregisterForGaiaCookieChanges();
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
#if defined(ENABLE_BASIC_PRINTING)
  web_ui()->RegisterMessageCallback("showSystemDialog",
      base::Bind(&PrintPreviewHandler::HandleShowSystemDialog,
                 base::Unretained(this)));
#endif  // ENABLE_BASIC_PRINTING
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
  web_ui()->RegisterMessageCallback("forceOpenNewTab",
      base::Bind(&PrintPreviewHandler::HandleForceOpenNewTab,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getPrivetPrinters",
      base::Bind(&PrintPreviewHandler::HandleGetPrivetPrinters,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("stopGetPrivetPrinters",
      base::Bind(&PrintPreviewHandler::HandleStopGetPrivetPrinters,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getPrivetPrinterCapabilities",
      base::Bind(&PrintPreviewHandler::HandleGetPrivetPrinterCapabilities,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensionPrinters",
      base::Bind(&PrintPreviewHandler::HandleGetExtensionPrinters,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensionPrinterCapabilities",
      base::Bind(&PrintPreviewHandler::HandleGetExtensionPrinterCapabilities,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "grantExtensionPrinterAccess",
      base::Bind(&PrintPreviewHandler::HandleGrantExtensionPrinterAccess,
                 base::Unretained(this)));
  RegisterForGaiaCookieChanges();
}

bool PrintPreviewHandler::PrivetPrintingEnabled() {
#if defined(ENABLE_SERVICE_DISCOVERY)
  return true;
#else
  return false;
#endif
}

WebContents* PrintPreviewHandler::preview_web_contents() const {
  return web_ui()->GetWebContents();
}

PrintPreviewUI* PrintPreviewHandler::print_preview_ui() const {
  return static_cast<PrintPreviewUI*>(web_ui()->GetController());
}

void PrintPreviewHandler::HandleGetPrinters(const base::ListValue* /*args*/) {
  base::ListValue* results = new base::ListValue;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&EnumeratePrintersOnFileThread,
                 base::Unretained(results)),
      base::Bind(&PrintPreviewHandler::SetupPrinterList,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(results)));
}

void PrintPreviewHandler::HandleGetPrivetPrinters(const base::ListValue* args) {
  if (!PrivetPrintingEnabled())
    return web_ui()->CallJavascriptFunction("onPrivetPrinterSearchDone");
#if defined(ENABLE_SERVICE_DISCOVERY)
  using local_discovery::ServiceDiscoverySharedClient;
  scoped_refptr<ServiceDiscoverySharedClient> service_discovery =
      ServiceDiscoverySharedClient::GetInstance();
  StartPrivetLister(service_discovery);
#endif  // ENABLE_SERVICE_DISCOVERY
}

void PrintPreviewHandler::HandleStopGetPrivetPrinters(
    const base::ListValue* args) {
#if defined(ENABLE_SERVICE_DISCOVERY)
  if (PrivetPrintingEnabled() && printer_lister_) {
    printer_lister_->Stop();
  }
#endif
}

void PrintPreviewHandler::HandleGetPrivetPrinterCapabilities(
    const base::ListValue* args) {
#if defined(ENABLE_SERVICE_DISCOVERY)
  std::string name;
  bool success = args->GetString(0, &name);
  DCHECK(success);

  CreatePrivetHTTP(
      name,
      base::Bind(&PrintPreviewHandler::PrivetCapabilitiesUpdateClient,
                 base::Unretained(this)));
#endif
}

void PrintPreviewHandler::HandleGetExtensionPrinters(
    const base::ListValue* args) {
  EnsureExtensionPrinterHandlerSet();
  // Make sure all in progress requests are canceled before new printer search
  // starts.
  extension_printer_handler_->Reset();
  extension_printer_handler_->StartGetPrinters(base::Bind(
      &PrintPreviewHandler::OnGotPrintersForExtension, base::Unretained(this)));
}

void PrintPreviewHandler::HandleGrantExtensionPrinterAccess(
    const base::ListValue* args) {
  std::string printer_id;
  bool ok = args->GetString(0, &printer_id);
  DCHECK(ok);

  EnsureExtensionPrinterHandlerSet();
  extension_printer_handler_->StartGrantPrinterAccess(
      printer_id, base::Bind(&PrintPreviewHandler::OnGotExtensionPrinterInfo,
                             base::Unretained(this), printer_id));
}

void PrintPreviewHandler::HandleGetExtensionPrinterCapabilities(
    const base::ListValue* args) {
  std::string printer_id;
  bool ok = args->GetString(0, &printer_id);
  DCHECK(ok);

  EnsureExtensionPrinterHandlerSet();
  extension_printer_handler_->StartGetCapability(
      printer_id,
      base::Bind(&PrintPreviewHandler::OnGotExtensionPrinterCapabilities,
                 base::Unretained(this)));
}

void PrintPreviewHandler::HandleGetPreview(const base::ListValue* args) {
  DCHECK_EQ(3U, args->GetSize());
  scoped_ptr<base::DictionaryValue> settings = GetSettingsDictionary(args);
  if (!settings)
    return;
  int request_id = -1;
  if (!settings->GetInteger(printing::kPreviewRequestID, &request_id))
    return;

  print_preview_ui()->OnPrintPreviewRequest(request_id);
  // Add an additional key in order to identify |print_preview_ui| later on
  // when calling PrintPreviewUI::GetCurrentPrintPreviewStatus() on the IO
  // thread.
  settings->SetInteger(printing::kPreviewUIID,
                       print_preview_ui()->GetIDForPrintPreviewUI());

  // Increment request count.
  ++regenerate_preview_request_count_;

  WebContents* initiator = GetInitiator();
  if (!initiator) {
    ReportUserActionHistogram(INITIATOR_CLOSED);
    print_preview_ui()->OnClosePrintPreviewDialog();
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
        initiator->GetController().GetLastCommittedEntry();
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
        print_preview_ui()->GetAvailableDraftPageCount() != draft_page_count) {
      settings->SetBoolean(printing::kSettingGenerateDraftData, true);
    }
  }

  VLOG(1) << "Print preview request start";

  bool distill_page = false;
  if (!settings->GetBoolean(printing::kSettingDistillPageEnabled,
                            &distill_page)) {
    NOTREACHED();
  }

  bool selection_only = false;
  if (!settings->GetBoolean(printing::kSettingShouldPrintSelectionOnly,
                            &selection_only)) {
    NOTREACHED();
  }

  if (distill_page && !selection_only) {
    print_preview_distiller_.reset(new PrintPreviewDistiller(
        initiator, base::Bind(&PrintPreviewUI::OnPrintPreviewFailed,
                              print_preview_ui()->GetWeakPtr()),
        std::move(settings)));
  } else {
    RenderViewHost* rvh = initiator->GetRenderViewHost();
    rvh->Send(new PrintMsg_PrintPreview(rvh->GetRoutingID(), *settings));
  }
}

void PrintPreviewHandler::HandlePrint(const base::ListValue* args) {
  ReportStats();

  // Record the number of times the user requests to regenerate preview data
  // before printing.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforePrint",
                       regenerate_preview_request_count_);

  scoped_ptr<base::DictionaryValue> settings = GetSettingsDictionary(args);
  if (!settings)
    return;

  ReportPrintSettingsStats(*settings);

  // Never try to add headers/footers here. It's already in the generated PDF.
  settings->SetBoolean(printing::kSettingHeaderFooterEnabled, false);

  bool print_to_pdf = false;
  bool is_cloud_printer = false;
  bool print_with_privet = false;
  bool print_with_extension = false;

  bool open_pdf_in_preview = false;
#if defined(OS_MACOSX)
  open_pdf_in_preview = settings->HasKey(printing::kSettingOpenPDFInPreview);
#endif

  if (!open_pdf_in_preview) {
    settings->GetBoolean(printing::kSettingPrintToPDF, &print_to_pdf);
    settings->GetBoolean(printing::kSettingPrintWithPrivet, &print_with_privet);
    settings->GetBoolean(printing::kSettingPrintWithExtension,
                         &print_with_extension);
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

#if defined(ENABLE_SERVICE_DISCOVERY)
  if (print_with_privet && PrivetPrintingEnabled()) {
    std::string printer_name;
    std::string print_ticket;
    std::string capabilities;
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintWithPrivet", page_count);
    ReportUserActionHistogram(PRINT_WITH_PRIVET);

    int width = 0;
    int height = 0;
    if (!settings->GetString(printing::kSettingDeviceName, &printer_name) ||
        !settings->GetString(printing::kSettingTicket, &print_ticket) ||
        !settings->GetString(printing::kSettingCapabilities, &capabilities) ||
        !settings->GetInteger(printing::kSettingPageWidth, &width) ||
        !settings->GetInteger(printing::kSettingPageHeight, &height) ||
        width <= 0 || height <= 0) {
      NOTREACHED();
      base::FundamentalValue http_code_value(-1);
      web_ui()->CallJavascriptFunction("onPrivetPrintFailed", http_code_value);
      return;
    }

    PrintToPrivetPrinter(
        printer_name, print_ticket, capabilities, gfx::Size(width, height));
    return;
  }
#endif

  if (print_with_extension) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintWithExtension",
                         page_count);
    ReportUserActionHistogram(PRINT_WITH_EXTENSION);

    std::string destination_id;
    std::string print_ticket;
    std::string capabilities;
    int width = 0;
    int height = 0;
    if (!settings->GetString(printing::kSettingDeviceName, &destination_id) ||
        !settings->GetString(printing::kSettingTicket, &print_ticket) ||
        !settings->GetString(printing::kSettingCapabilities, &capabilities) ||
        !settings->GetInteger(printing::kSettingPageWidth, &width) ||
        !settings->GetInteger(printing::kSettingPageHeight, &height) ||
        width <= 0 || height <= 0) {
      NOTREACHED();
      OnExtensionPrintResult(false, "FAILED");
      return;
    }

    base::string16 title;
    scoped_refptr<base::RefCountedBytes> data;
    if (!GetPreviewDataAndTitle(&data, &title)) {
      LOG(ERROR) << "Nothing to print; no preview available.";
      OnExtensionPrintResult(false, "NO_DATA");
      return;
    }

    EnsureExtensionPrinterHandlerSet();
    extension_printer_handler_->StartPrint(
        destination_id, capabilities, title, print_ticket,
        gfx::Size(width, height), data,
        base::Bind(&PrintPreviewHandler::OnExtensionPrintResult,
                   base::Unretained(this)));
    return;
  }

  scoped_refptr<base::RefCountedBytes> data;
  base::string16 title;
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
#if defined(ENABLE_BASIC_PRINTING)
    bool system_dialog = false;
    settings->GetBoolean(printing::kSettingShowSystemDialog, &system_dialog);
    if (system_dialog) {
      UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.SystemDialog", page_count);
      ReportUserActionHistogram(FALLBACK_TO_ADVANCED_SETTINGS_DIALOG);
    } else {
      UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPrinter", page_count);
      ReportUserActionHistogram(PRINT_TO_PRINTER);
    }

    // This tries to activate the initiator as well, so do not clear the
    // association with the initiator yet.
    print_preview_ui()->OnHidePreviewDialog();

    // Do this so the initiator can open a new print preview dialog, while the
    // current print preview dialog is still handling its print job.
    WebContents* initiator = GetInitiator();
    if (initiator) {
      // Save initiator IDs. |PrintingMessageFilter::OnUpdatePrintSettings|
      // would be called when initiator info is cleared.
      settings->SetInteger(printing::kPreviewInitiatorHostId,
                           initiator->GetRenderProcessHost()->GetID());
      settings->SetInteger(printing::kPreviewInitiatorRoutingId,
                           initiator->GetRoutingID());
    }

    ClearInitiatorDetails();

    // The PDF being printed contains only the pages that the user selected,
    // so ignore the page range and print all pages.
    settings->Remove(printing::kSettingPageRange, NULL);
    // Reset selection only flag for the same reason.
    settings->SetBoolean(printing::kSettingShouldPrintSelectionOnly, false);

    // Set ID to know whether printing is for preview.
    settings->SetInteger(printing::kPreviewUIID,
                         print_preview_ui()->GetIDForPrintPreviewUI());
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
#else
    NOTREACHED();
#endif   // defined(ENABLE_BASIC_PRINTING)
  }
}

void PrintPreviewHandler::PrintToPdf() {
  if (!print_to_pdf_path_.empty()) {
    // User has already selected a path, no need to show the dialog again.
    PostPrintToPdfTask();
  } else if (!select_file_dialog_.get() ||
             !select_file_dialog_->IsRunning(platform_util::GetTopLevel(
                 preview_web_contents()->GetNativeView()))) {
    // Pre-populating select file dialog with print job title.
    base::string16 print_job_title_utf16 =
        print_preview_ui()->initiator_title();

#if defined(OS_WIN)
    base::FilePath::StringType print_job_title(print_job_title_utf16);
#elif defined(OS_POSIX)
    base::FilePath::StringType print_job_title =
        base::UTF16ToUTF8(print_job_title_utf16);
#endif

    base::i18n::ReplaceIllegalCharactersInPath(&print_job_title, '_');
    base::FilePath default_filename(print_job_title);
    default_filename =
        default_filename.ReplaceExtension(FILE_PATH_LITERAL("pdf"));

    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    bool prompt_user = !cmdline->HasSwitch(switches::kKioskModePrinting);
    SelectFile(default_filename, prompt_user);
  }
}

void PrintPreviewHandler::HandleHidePreview(const base::ListValue* /*args*/) {
  print_preview_ui()->OnHidePreviewDialog();
}

void PrintPreviewHandler::HandleCancelPendingPrintRequest(
    const base::ListValue* /*args*/) {
  WebContents* initiator = GetInitiator();
  if (initiator)
    ClearInitiatorDetails();
  chrome::ShowPrintErrorDialog();
}

void PrintPreviewHandler::HandleSaveAppState(const base::ListValue* args) {
  std::string data_to_save;
  printing::StickySettings* sticky_settings = GetStickySettings();
  if (args->GetString(0, &data_to_save) && !data_to_save.empty())
    sticky_settings->StoreAppState(data_to_save);
  sticky_settings->SaveInPrefs(Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext())->GetPrefs());
}

void PrintPreviewHandler::HandleGetPrinterCapabilities(
    const base::ListValue* args) {
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
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&GetPrinterCapabilitiesOnFileThread,
                                     printer_name,
                                     g_browser_process->GetApplicationLocale(),
                                     success_cb, failure_cb));
}

void PrintPreviewHandler::OnSigninComplete() {
  if (print_preview_ui())
    print_preview_ui()->OnReloadPrintersList();
}

void PrintPreviewHandler::HandleSignin(const base::ListValue* args) {
  bool add_account = false;
  bool success = args->GetBoolean(0, &add_account);
  DCHECK(success);

  Profile* profile = Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  print_dialog_cloud::CreateCloudPrintSigninTab(
      displayer.browser(),
      add_account,
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

void PrintPreviewHandler::HandleManageCloudPrint(
    const base::ListValue* args) {
  ++manage_cloud_printers_dialog_request_count_;
  GURL manage_url(cloud_devices::GetCloudPrintRelativeURL("manage.html"));
  std::string user;
  if (!args->GetString(0, &user))
    return;
  if (!user.empty())
    manage_url = net::AppendQueryParameter(manage_url, "user", user);
  preview_web_contents()->OpenURL(content::OpenURLParams(
      manage_url,
      content::Referrer(),
      NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK,
      false));
}

#if defined(ENABLE_BASIC_PRINTING)
void PrintPreviewHandler::HandleShowSystemDialog(
    const base::ListValue* /*args*/) {
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
  print_preview_ui()->OnCancelPendingPreviewRequest();
}
#endif  // ENABLE_BASIC_PRINTING

void PrintPreviewHandler::HandleManagePrinters(
    const base::ListValue* /*args*/) {
  ++manage_printers_dialog_request_count_;
  printing::PrinterManagerDialog::ShowPrinterManagerDialog();
}

void PrintPreviewHandler::HandleClosePreviewDialog(
    const base::ListValue* /*args*/) {
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
  // On error, assume the units are SI.
  // Since the only measurement units print preview's WebUI cares about are
  // those for measuring distance, assume anything non-US is SI.
  if (errorCode > U_ZERO_ERROR || system != UMS_US)
    system = UMS_SI;

  // Getting the number formatting based on the locale and writing to
  // dictionary.
  settings->SetString(kNumberFormat, base::FormatDouble(123456.78, 2));
  settings->SetInteger(kMeasurementSystem, system);
}

void PrintPreviewHandler::HandleGetInitialSettings(
    const base::ListValue* /*args*/) {
  // Send before SendInitialSettings to allow cloud printer auto select.
  SendCloudPrintEnabled();
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GetDefaultPrinterOnFileThread),
      base::Bind(&PrintPreviewHandler::SendInitialSettings,
                 weak_factory_.GetWeakPtr()));
}

void PrintPreviewHandler::HandleForceOpenNewTab(const base::ListValue* args) {
  std::string url;
  if (!args->GetString(0, &url))
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(GetInitiator());
  if (!browser)
    return;
  chrome::AddSelectedTabWithURL(browser,
                                GURL(url),
                                ui::PAGE_TRANSITION_LINK);
}

void PrintPreviewHandler::SendInitialSettings(
    const std::string& default_printer) {
  base::DictionaryValue initial_settings;
  initial_settings.SetString(kInitiatorTitle,
                             print_preview_ui()->initiator_title());
  initial_settings.SetBoolean(printing::kSettingPreviewModifiable,
                              print_preview_ui()->source_is_modifiable());
  initial_settings.SetString(printing::kSettingPrinterName, default_printer);
  initial_settings.SetBoolean(kDocumentHasSelection,
                              print_preview_ui()->source_has_selection());
  initial_settings.SetBoolean(printing::kSettingShouldPrintSelectionOnly,
                              print_preview_ui()->print_selection_only());
  PrefService* prefs = Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext())->GetPrefs();
  printing::StickySettings* sticky_settings = GetStickySettings();
  sticky_settings->RestoreFromPrefs(prefs);
  if (sticky_settings->printer_app_state()) {
    initial_settings.SetString(kAppState,
                               *sticky_settings->printer_app_state());
  }

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  initial_settings.SetBoolean(kPrintAutomaticallyInKioskMode,
                              cmdline->HasSwitch(switches::kKioskModePrinting));
  initial_settings.SetBoolean(kAppKioskMode,
                              chrome::IsRunningInForcedAppMode());
#if defined(OS_WIN)
  // In Win8 metro, the system print dialog can only open on the desktop.  Doing
  // so will cause the browser to appear hung, so we don't show the link in
  // metro.
  bool is_ash = (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH);
  initial_settings.SetBoolean(kHidePrintWithSystemDialogLink, is_ash);
#endif
  if (prefs) {
    const std::string rules_str =
        prefs->GetString(prefs::kPrintPreviewDefaultDestinationSelectionRules);
    if (!rules_str.empty())
      initial_settings.SetString(kDefaultDestinationSelectionRules, rules_str);
  }

  if (print_preview_ui()->source_is_modifiable())
    GetNumberFormatAndMeasurementSystem(&initial_settings);
  web_ui()->CallJavascriptFunction("setInitialSettings", initial_settings);

  if (PrintPreviewDistiller::IsEnabled()) {
    using dom_distiller::url_utils::IsUrlDistillable;
    WebContents* initiator = GetInitiator();
    if (initiator && IsUrlDistillable(initiator->GetLastCommittedURL()))
      web_ui()->CallJavascriptFunction("allowDistillPage");
  }
}

void PrintPreviewHandler::ClosePreviewDialog() {
  print_preview_ui()->OnClosePrintPreviewDialog();
}

void PrintPreviewHandler::SendAccessToken(const std::string& type,
                                          const std::string& access_token) {
  VLOG(1) << "Get getAccessToken finished";
  web_ui()->CallJavascriptFunction("onDidGetAccessToken",
                                   base::StringValue(type),
                                   base::StringValue(access_token));
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const base::DictionaryValue* settings_info) {
  VLOG(1) << "Get printer capabilities finished";
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
    base::DictionaryValue settings;
    settings.SetString(kCloudPrintUrl,
                       GURL(cloud_devices::GetCloudPrintURL()).spec());
    settings.SetBoolean(kAppKioskMode, chrome::IsRunningInForcedAppMode());
    web_ui()->CallJavascriptFunction("setUseCloudPrint", settings);
  }
}

void PrintPreviewHandler::SendCloudPrintJob(const base::RefCountedBytes* data) {
  // BASE64 encode the job data.
  std::string raw_data(reinterpret_cast<const char*>(data->front()),
                       data->size());
  std::string base64_data;
  base::Base64Encode(raw_data, &base64_data);
  base::StringValue data_value(base64_data);

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

void PrintPreviewHandler::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  OnSigninComplete();
}

void PrintPreviewHandler::SelectFile(const base::FilePath& default_filename,
                                     bool prompt_user) {
  if (prompt_user) {
    ChromeSelectFilePolicy policy(GetInitiator());
    if (!policy.CanOpenSelectFileDialog()) {
      policy.SelectFileDenied();
      return ClosePreviewDialog();
    }
  }

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

  // Handle the no prompting case. Like the dialog prompt, this function
  // returns and eventually FileSelected() gets called.
  if (!prompt_user) {
    base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&GetUniquePath,
                   sticky_settings->save_path()->Append(default_filename)),
        base::Bind(&PrintPreviewHandler::OnGotUniqueFileName,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  // Otherwise prompt the user.
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pdf"));

  select_file_dialog_ =
      ui::SelectFileDialog::Create(this, nullptr /*policy already checked*/);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      base::string16(),
      sticky_settings->save_path()->Append(default_filename),
      &file_type_info,
      0,
      base::FilePath::StringType(),
      platform_util::GetTopLevel(preview_web_contents()->GetNativeView()),
      NULL);
}

void PrintPreviewHandler::OnGotUniqueFileName(const base::FilePath& path) {
  FileSelected(path, 0, nullptr);
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

#if defined(ENABLE_BASIC_PRINTING)
void PrintPreviewHandler::ShowSystemDialog() {
  HandleShowSystemDialog(NULL);
}
#endif  // ENABLE_BASIC_PRINTING

void PrintPreviewHandler::FileSelected(const base::FilePath& path,
                                       int /* index */,
                                       void* /* params */) {
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
  base::string16 title;
  if (!GetPreviewDataAndTitle(&data, &title)) {
    NOTREACHED() << "Preview data was checked before file dialog.";
    return;
  }
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&PrintToPdfCallback,
                                     data,
                                     print_to_pdf_path_,
                                     pdf_file_saved_closure_));
  print_to_pdf_path_ = base::FilePath();
  ClosePreviewDialog();
}

void PrintPreviewHandler::FileSelectionCanceled(void* params) {
  print_preview_ui()->OnFileSelectionCancelled();
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
    base::string16* title) const {
  scoped_refptr<base::RefCountedBytes> tmp_data;
  print_preview_ui()->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX, &tmp_data);

  if (!tmp_data.get()) {
    // Nothing to print, no preview available.
    return false;
  }
  DCHECK(tmp_data->size() && tmp_data->front());

  *data = tmp_data;
  *title = print_preview_ui()->initiator_title();
  return true;
}

#if defined(ENABLE_SERVICE_DISCOVERY)

void PrintPreviewHandler::StartPrivetLister(const scoped_refptr<
    local_discovery::ServiceDiscoverySharedClient>& client) {
  if (!PrivetPrintingEnabled())
    return web_ui()->CallJavascriptFunction("onPrivetPrinterSearchDone");

  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(!service_discovery_client_.get() ||
         service_discovery_client_.get() == client.get());
  service_discovery_client_ = client;
  printer_lister_.reset(new cloud_print::PrivetLocalPrinterLister(
      service_discovery_client_.get(), profile->GetRequestContext(), this));
  printer_lister_->Start();
}

void PrintPreviewHandler::LocalPrinterChanged(
    bool added,
    const std::string& name,
    bool has_local_printing,
    const cloud_print::DeviceDescription& description) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (has_local_printing ||
      command_line->HasSwitch(switches::kEnablePrintPreviewRegisterPromos)) {
    base::DictionaryValue info;
    FillPrinterDescription(name, description, has_local_printing, &info);
    web_ui()->CallJavascriptFunction("onPrivetPrinterChanged", info);
  }
}

void PrintPreviewHandler::LocalPrinterRemoved(const std::string& name) {
}

void PrintPreviewHandler::LocalPrinterCacheFlushed() {
}

void PrintPreviewHandler::PrivetCapabilitiesUpdateClient(
    scoped_ptr<cloud_print::PrivetHTTPClient> http_client) {
  if (!PrivetUpdateClient(std::move(http_client)))
    return;

  privet_capabilities_operation_ =
      privet_http_client_->CreateCapabilitiesOperation(
          base::Bind(&PrintPreviewHandler::OnPrivetCapabilities,
                     base::Unretained(this)));
  privet_capabilities_operation_->Start();
}

bool PrintPreviewHandler::PrivetUpdateClient(
    scoped_ptr<cloud_print::PrivetHTTPClient> http_client) {
  if (!http_client) {
    SendPrivetCapabilitiesError(privet_http_resolution_->GetName());
    privet_http_resolution_.reset();
    return false;
  }

  privet_local_print_operation_.reset();
  privet_capabilities_operation_.reset();
  privet_http_client_ = cloud_print::PrivetV1HTTPClient::CreateDefault(
      std::move(http_client));

  privet_http_resolution_.reset();

  return true;
}

void PrintPreviewHandler::PrivetLocalPrintUpdateClient(
    std::string print_ticket,
    std::string capabilities,
    gfx::Size page_size,
    scoped_ptr<cloud_print::PrivetHTTPClient> http_client) {
  if (!PrivetUpdateClient(std::move(http_client)))
    return;

  StartPrivetLocalPrint(print_ticket, capabilities, page_size);
}

void PrintPreviewHandler::StartPrivetLocalPrint(const std::string& print_ticket,
                                                const std::string& capabilities,
                                                const gfx::Size& page_size) {
  privet_local_print_operation_ =
      privet_http_client_->CreateLocalPrintOperation(this);

  privet_local_print_operation_->SetTicket(print_ticket);
  privet_local_print_operation_->SetCapabilities(capabilities);

  scoped_refptr<base::RefCountedBytes> data;
  base::string16 title;

  if (!GetPreviewDataAndTitle(&data, &title)) {
    base::FundamentalValue http_code_value(-1);
    web_ui()->CallJavascriptFunction("onPrivetPrintFailed", http_code_value);
    return;
  }

  privet_local_print_operation_->SetJobname(base::UTF16ToUTF8(title));
  privet_local_print_operation_->SetPageSize(page_size);
  privet_local_print_operation_->SetData(data.get());

  Profile* profile = Profile::FromWebUI(web_ui());
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile);

  if (signin_manager) {
    privet_local_print_operation_->SetUsername(
        signin_manager->GetAuthenticatedAccountInfo().email);
  }

  privet_local_print_operation_->Start();
}


void PrintPreviewHandler::OnPrivetCapabilities(
    const base::DictionaryValue* capabilities) {
  std::string name = privet_capabilities_operation_->GetHTTPClient()->GetName();

  if (!capabilities || capabilities->HasKey(cloud_print::kPrivetKeyError) ||
      !printer_lister_) {
    SendPrivetCapabilitiesError(name);
    return;
  }

  base::DictionaryValue printer_info;
  const cloud_print::DeviceDescription* description =
      printer_lister_->GetDeviceDescription(name);

  if (!description) {
    SendPrivetCapabilitiesError(name);
    return;
  }

  FillPrinterDescription(name, *description, true, &printer_info);

  web_ui()->CallJavascriptFunction(
      "onPrivetCapabilitiesSet",
      printer_info,
      *capabilities);

  privet_capabilities_operation_.reset();
}

void PrintPreviewHandler::SendPrivetCapabilitiesError(
    const std::string& device_name) {
  base::StringValue name_value(device_name);
  web_ui()->CallJavascriptFunction(
      "failedToGetPrivetPrinterCapabilities",
      name_value);
}

void PrintPreviewHandler::PrintToPrivetPrinter(const std::string& device_name,
                                               const std::string& ticket,
                                               const std::string& capabilities,
                                               const gfx::Size& page_size) {
  CreatePrivetHTTP(
      device_name,
      base::Bind(&PrintPreviewHandler::PrivetLocalPrintUpdateClient,
                 base::Unretained(this),
                 ticket,
                 capabilities,
                 page_size));
}

bool PrintPreviewHandler::CreatePrivetHTTP(
    const std::string& name,
    const cloud_print::PrivetHTTPAsynchronousFactory::ResultCallback&
        callback) {
  const cloud_print::DeviceDescription* device_description =
      printer_lister_ ? printer_lister_->GetDeviceDescription(name) : NULL;

  if (!device_description) {
    SendPrivetCapabilitiesError(name);
    return false;
  }

  privet_http_factory_ =
      cloud_print::PrivetHTTPAsynchronousFactory::CreateInstance(
          Profile::FromWebUI(web_ui())->GetRequestContext());
  privet_http_resolution_ = privet_http_factory_->CreatePrivetHTTP(name);
  privet_http_resolution_->Start(device_description->address, callback);

  return true;
}

void PrintPreviewHandler::OnPrivetPrintingDone(
    const cloud_print::PrivetLocalPrintOperation* print_operation) {
  ClosePreviewDialog();
}

void PrintPreviewHandler::OnPrivetPrintingError(
    const cloud_print::PrivetLocalPrintOperation* print_operation,
    int http_code) {
  base::FundamentalValue http_code_value(http_code);
  web_ui()->CallJavascriptFunction("onPrivetPrintFailed", http_code_value);
}

void PrintPreviewHandler::FillPrinterDescription(
    const std::string& name,
    const cloud_print::DeviceDescription& description,
    bool has_local_printing,
    base::DictionaryValue* printer_value) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  printer_value->SetString("serviceName", name);
  printer_value->SetString("name", description.name);
  printer_value->SetBoolean("hasLocalPrinting", has_local_printing);
  printer_value->SetBoolean(
      "isUnregistered",
      description.id.empty() &&
      command_line->HasSwitch(switches::kEnablePrintPreviewRegisterPromos));
  printer_value->SetString("cloudID", description.id);
}

#endif  // defined(ENABLE_SERVICE_DISCOVERY)

void PrintPreviewHandler::EnsureExtensionPrinterHandlerSet() {
  if (extension_printer_handler_)
    return;

  extension_printer_handler_ =
      PrinterHandler::CreateForExtensionPrinters(Profile::FromWebUI(web_ui()));
}

void PrintPreviewHandler::OnGotPrintersForExtension(
    const base::ListValue& printers,
    bool done) {
  web_ui()->CallJavascriptFunction("onExtensionPrintersAdded", printers,
                                   base::FundamentalValue(done));
}

void PrintPreviewHandler::OnGotExtensionPrinterInfo(
    const std::string& printer_id,
    const base::DictionaryValue& printer_info) {
  if (printer_info.empty()) {
    web_ui()->CallJavascriptFunction("failedToResolveProvisionalPrinter",
                                     base::StringValue(printer_id));
    return;
  }

  web_ui()->CallJavascriptFunction("onProvisionalPrinterResolved",
                                   base::StringValue(printer_id),
                                   printer_info);
}

void PrintPreviewHandler::OnGotExtensionPrinterCapabilities(
    const std::string& printer_id,
    const base::DictionaryValue& capabilities) {
  if (capabilities.empty()) {
    web_ui()->CallJavascriptFunction("failedToGetExtensionPrinterCapabilities",
                                     base::StringValue(printer_id));
    return;
  }

  web_ui()->CallJavascriptFunction("onExtensionCapabilitiesSet",
                                   base::StringValue(printer_id), capabilities);
}

void PrintPreviewHandler::OnExtensionPrintResult(bool success,
                                                 const std::string& status) {
  if (success) {
    ClosePreviewDialog();
    return;
  }

  // TODO(tbarzic): This function works for extension printers case too, but it
  // should be renamed to something more generic.
  web_ui()->CallJavascriptFunction("onPrivetPrintFailed",
                                   base::StringValue(status));
}

void PrintPreviewHandler::RegisterForGaiaCookieChanges() {
  DCHECK(!gaia_cookie_manager_service_);
  Profile* profile = Profile::FromWebUI(web_ui());
  if (switches::IsEnableAccountConsistency() && !profile->IsOffTheRecord()) {
    gaia_cookie_manager_service_ =
        GaiaCookieManagerServiceFactory::GetForProfile(profile);
    if (gaia_cookie_manager_service_)
      gaia_cookie_manager_service_->AddObserver(this);
  }
}

void PrintPreviewHandler::UnregisterForGaiaCookieChanges() {
  if (gaia_cookie_manager_service_)
    gaia_cookie_manager_service_->RemoveObserver(this);
}

void PrintPreviewHandler::SetPdfSavedClosureForTesting(
    const base::Closure& closure) {
  pdf_file_saved_closure_ = closure;
}
