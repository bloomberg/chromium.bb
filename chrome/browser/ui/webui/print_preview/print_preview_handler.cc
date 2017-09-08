// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <ctype.h>
#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
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
#include "chrome/browser/ui/webui/print_preview/printer_backend_proxy.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_cdd_conversion.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/cloud_devices/common/printer_description.h"
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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/url_util.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/features/features.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"
#include "printing/units.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chromeos/printing/printer_configuration.h"
#endif

using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;
using printing::PrintViewManager;
using printing::PrinterType;

namespace {

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum UserActionBuckets {
  PRINT_TO_PRINTER,
  PRINT_TO_PDF,
  CANCEL,
  FALLBACK_TO_ADVANCED_SETTINGS_DIALOG,
  PREVIEW_FAILED,
  PREVIEW_STARTED,
  INITIATOR_CRASHED_UNUSED,
  INITIATOR_CLOSED,
  PRINT_WITH_CLOUD_PRINT,
  PRINT_WITH_PRIVET,
  PRINT_WITH_EXTENSION,
  USERACTION_BUCKET_BOUNDARY
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
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
  DISTILL_PAGE_UNUSED,
  SCALING,
  PRINT_AS_IMAGE,
  PRINT_SETTINGS_BUCKET_BOUNDARY
};

// This enum is used to back an UMA histogram, and should therefore be treated
// as append only.
enum PrintDocumentTypeBuckets {
  HTML_DOCUMENT = 0,
  PDF_DOCUMENT,
  PRINT_DOCUMENT_TYPE_BUCKET_BOUNDARY
};

void ReportUserActionHistogram(enum UserActionBuckets event) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.UserAction", event,
                            USERACTION_BUCKET_BOUNDARY);
}

void ReportPrintSettingHistogram(enum PrintSettingsBuckets setting) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.PrintSettings", setting,
                            PRINT_SETTINGS_BUCKET_BOUNDARY);
}

void ReportPrintDocumentTypeHistogram(enum PrintDocumentTypeBuckets doctype) {
  UMA_HISTOGRAM_ENUMERATION("PrintPreview.PrintDocumentType", doctype,
                            PRINT_DOCUMENT_TYPE_BUCKET_BOUNDARY);
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
// Name of a dictionary field holding the state of selection for document.
const char kDocumentHasSelection[] = "documentHasSelection";
// Dictionary field holding the default destination selection rules.
const char kDefaultDestinationSelectionRules[] =
    "defaultDestinationSelectionRules";

// Id of the predefined PDF printer.
const char kLocalPdfPrinterId[] = "Save as PDF";

// Get the print job settings dictionary from |json_str|. Returns NULL on
// failure.
std::unique_ptr<base::DictionaryValue> GetSettingsDictionary(
    const std::string& json_str) {
  if (json_str.empty()) {
    NOTREACHED() << "Empty print job settings";
    return NULL;
  }
  std::unique_ptr<base::DictionaryValue> settings =
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

  int scaling = 100;
  if (settings.GetInteger(printing::kSettingScaleFactor, &scaling) &&
      scaling != 100) {
    ReportPrintSettingHistogram(SCALING);
  }

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

  bool rasterize = false;
  if (settings.GetBoolean(printing::kSettingRasterizePdf,
                          &rasterize) && rasterize) {
    ReportPrintSettingHistogram(PRINT_AS_IMAGE);
  }
}

// Callback that stores a PDF file on disk.
void PrintToPdfCallback(const scoped_refptr<base::RefCountedBytes>& data,
                        const base::FilePath& path,
                        const base::Closure& pdf_file_saved_closure) {
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(reinterpret_cast<const char*>(data->front()),
                         base::checked_cast<int>(data->size()));
  if (!pdf_file_saved_closure.is_null())
    pdf_file_saved_closure.Run();
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
  std::unique_ptr<printing::PrintingContext> printing_context(
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

std::unique_ptr<base::DictionaryValue> GetPdfCapabilities(
    const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

  return std::unique_ptr<base::DictionaryValue>(description.root().DeepCopy());
}

void PrintersToValues(const printing::PrinterList& printer_list,
                      base::ListValue* printers) {
  for (const printing::PrinterBasicInfo& printer : printer_list) {
    auto printer_info = base::MakeUnique<base::DictionaryValue>();
    printer_info->SetString(printing::kSettingDeviceName, printer.printer_name);

    const auto printer_name_description =
        printing::GetPrinterNameAndDescription(printer);
    const std::string& printer_name = printer_name_description.first;
    const std::string& printer_description = printer_name_description.second;
    printer_info->SetString(printing::kSettingPrinterName, printer_name);
    printer_info->SetString(printing::kSettingPrinterDescription,
                            printer_description);

    auto options = base::MakeUnique<base::DictionaryValue>();
    for (const auto opt_it : printer.options)
      options->SetString(opt_it.first, opt_it.second);

    printer_info->SetBoolean(
        kCUPSEnterprisePrinter,
        base::ContainsKey(printer.options, kCUPSEnterprisePrinter) &&
            printer.options.at(kCUPSEnterprisePrinter) == kValueTrue);

    printer_info->Set(printing::kSettingPrinterOptions, std::move(options));

    printers->Append(std::move(printer_info));

    VLOG(1) << "Found printer " << printer_name << " with device name "
            << printer.printer_name;
  }
}

base::LazyInstance<printing::StickySettings>::DestructorAtExit
    g_sticky_settings = LAZY_INSTANCE_INITIALIZER;

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

void CreateDirectoryIfNeeded(const base::FilePath& path) {
  if (!base::DirectoryExists(path))
    base::CreateDirectory(path);
}

}  // namespace

class PrintPreviewHandler::AccessTokenService
    : public OAuth2TokenService::Consumer {
 public:
  explicit AccessTokenService(PrintPreviewHandler* handler)
      : OAuth2TokenService::Consumer("print_preview"),
        handler_(handler) {
  }

  void RequestToken(const std::string& type, const std::string& callback_id) {
    if (requests_.find(type) != requests_.end())
      return;  // Should never happen, see cloud_print_interface.js

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
      std::unique_ptr<OAuth2TokenService::Request> request(
          service->StartRequest(account_id, oauth_scopes, this));
      requests_[type] = std::move(request);
      callbacks_[type] = callback_id;
    } else {
      // Unknown type.
      handler_->SendAccessToken(callback_id, std::string());
    }
  }

  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {
    OnServiceResponse(request, access_token);
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    OnServiceResponse(request, std::string());
  }

 private:
  void OnServiceResponse(const OAuth2TokenService::Request* request,
                         const std::string& access_token) {
    for (Requests::iterator i = requests_.begin(); i != requests_.end(); ++i) {
      if (i->second.get() == request) {
        handler_->SendAccessToken(callbacks_[i->first], access_token);
        requests_.erase(i);
        callbacks_.erase(i->first);
        return;
      }
    }
    NOTREACHED();
  }

  using Requests =
      std::map<std::string, std::unique_ptr<OAuth2TokenService::Request>>;
  Requests requests_;
  using Callbacks = std::map<std::string, std::string>;
  Callbacks callbacks_;
  PrintPreviewHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenService);
};

PrintPreviewHandler::PrintPreviewHandler()
    : regenerate_preview_request_count_(0),
      manage_printers_dialog_request_count_(0),
      manage_cloud_printers_dialog_request_count_(0),
      reported_failed_preview_(false),
      has_logged_printers_count_(false),
      gaia_cookie_manager_service_(nullptr),
      printer_backend_proxy_(nullptr),
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
  web_ui()->RegisterMessageCallback(
      "getPrinterCapabilities",
      base::Bind(&PrintPreviewHandler::HandleGetPrinterCapabilities,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setupPrinter", base::Bind(&PrintPreviewHandler::HandlePrinterSetup,
                                 base::Unretained(this)));
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  web_ui()->RegisterMessageCallback("showSystemDialog",
      base::Bind(&PrintPreviewHandler::HandleShowSystemDialog,
                 base::Unretained(this)));
#endif
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
  web_ui()->RegisterMessageCallback(
      "getExtensionOrPrivetPrinters",
      base::Bind(&PrintPreviewHandler::HandleGetExtensionOrPrivetPrinters,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensionOrPrivetPrinterCapabilities",
      base::Bind(
          &PrintPreviewHandler::HandleGetExtensionOrPrivetPrinterCapabilities,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "grantExtensionPrinterAccess",
      base::Bind(&PrintPreviewHandler::HandleGrantExtensionPrinterAccess,
                 base::Unretained(this)));
}

void PrintPreviewHandler::OnJavascriptAllowed() {
  // Now that the UI is initialized, any future account changes will require
  // a printer list refresh.
  RegisterForGaiaCookieChanges();
}

void PrintPreviewHandler::OnJavascriptDisallowed() {
  // Normally the handler and print preview will be destroyed together, but
  // this is necessary for refresh or navigation from the chrome://print page.
  weak_factory_.InvalidateWeakPtrs();
  UnregisterForGaiaCookieChanges();
}

WebContents* PrintPreviewHandler::preview_web_contents() const {
  return web_ui()->GetWebContents();
}

PrintPreviewUI* PrintPreviewHandler::print_preview_ui() const {
  return static_cast<PrintPreviewUI*>(web_ui()->GetController());
}

printing::PrinterBackendProxy* PrintPreviewHandler::printer_backend_proxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!printer_backend_proxy_) {
#if defined(OS_CHROMEOS)
    // ChromeOS stores printer information in printer prefs which requires a
    // profile.  Other plaforms retrieve printer information from OS resources.
    printer_backend_proxy_ =
        printing::PrinterBackendProxy::Create(Profile::FromWebUI(web_ui()));
#else
    printer_backend_proxy_ = printing::PrinterBackendProxy::Create();
#endif
  }

  return printer_backend_proxy_.get();
}

void PrintPreviewHandler::HandleGetPrinters(const base::ListValue* args) {
  VLOG(1) << "Enumerate printers start";
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  printer_backend_proxy()->EnumeratePrinters(
      base::Bind(&PrintPreviewHandler::SetupPrinterList,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetExtensionOrPrivetPrinters(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  int type;
  CHECK(args->GetInteger(1, &type));
  PrinterType printer_type = static_cast<PrinterType>(type);

  PrinterHandler* handler = GetPrinterHandler(printer_type);
  if (!handler) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  // Make sure all in progress requests are canceled before new printer search
  // starts.
  handler->Reset();
  handler->StartGetPrinters(
      base::Bind(&PrintPreviewHandler::OnGotPrintersForExtensionOrPrivet,
                 weak_factory_.GetWeakPtr(), callback_id, printer_type));
}

void PrintPreviewHandler::HandleGrantExtensionPrinterAccess(
    const base::ListValue* args) {
  std::string callback_id;
  std::string printer_id;
  bool ok = args->GetString(0, &callback_id) &&
            args->GetString(1, &printer_id) && !callback_id.empty();
  DCHECK(ok);

  GetPrinterHandler(PrinterType::kExtensionPrinter)
      ->StartGrantPrinterAccess(
          printer_id,
          base::Bind(&PrintPreviewHandler::OnGotExtensionPrinterInfo,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetExtensionOrPrivetPrinterCapabilities(
    const base::ListValue* args) {
  std::string callback_id;
  std::string printer_name;
  int type;
  if (!args->GetString(0, &callback_id) || !args->GetString(1, &printer_name) ||
      !args->GetInteger(2, &type) || callback_id.empty() ||
      printer_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  PrinterType printer_type = static_cast<PrinterType>(type);

  PrinterHandler* handler = GetPrinterHandler(printer_type);
  if (!handler) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  handler->StartGetCapability(
      printer_name, base::Bind(&PrintPreviewHandler::OnGotPrinterCapabilities,
                               weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetPreview(const base::ListValue* args) {
  DCHECK_EQ(3U, args->GetSize());
  std::string callback_id;
  std::string json_str;

  // All of the conditions below should be guaranteed by the print preview
  // javascript.
  args->GetString(0, &callback_id);
  CHECK(!callback_id.empty());
  args->GetString(1, &json_str);
  std::unique_ptr<base::DictionaryValue> settings =
      GetSettingsDictionary(json_str);
  CHECK(settings);
  int request_id = -1;
  settings->GetInteger(printing::kPreviewRequestID, &request_id);
  CHECK_GT(request_id, -1);

  preview_callbacks_.push(callback_id);
  print_preview_ui()->OnPrintPreviewRequest(request_id);
  // Add an additional key in order to identify |print_preview_ui| later on
  // when calling PrintPreviewUI::GetCurrentPrintPreviewStatus() on the IO
  // thread.
  settings->SetInteger(printing::kPreviewUIID,
                       print_preview_ui()->GetIDForPrintPreviewUI());

  // Increment request count.
  ++regenerate_preview_request_count_;

  WebContents* initiator = GetInitiator();
  content::RenderFrameHost* rfh =
      initiator
          ? PrintViewManager::FromWebContents(initiator)->print_preview_rfh()
          : nullptr;
  if (!rfh) {
    ReportUserActionHistogram(INITIATOR_CLOSED);
    print_preview_ui()->OnClosePrintPreviewDialog();
    return;
  }

  // Retrieve the page title and url and send it to the renderer process if
  // headers and footers are to be displayed.
  bool display_header_footer = false;
  bool success = settings->GetBoolean(printing::kSettingHeaderFooterEnabled,
                                      &display_header_footer);
  DCHECK(success);
  if (display_header_footer) {
    settings->SetString(printing::kSettingHeaderFooterTitle,
                        initiator->GetTitle());
    std::string url;
    content::NavigationEntry* entry =
        initiator->GetController().GetLastCommittedEntry();
    if (entry) {
      url::Replacements<char> url_sanitizer;
      url_sanitizer.ClearUsername();
      url_sanitizer.ClearPassword();

      url = entry->GetVirtualURL().ReplaceComponents(url_sanitizer).spec();
    }
    settings->SetString(printing::kSettingHeaderFooterURL, url);
  }

  bool generate_draft_data = false;
  success = settings->GetBoolean(printing::kSettingGenerateDraftData,
                                 &generate_draft_data);
  DCHECK(success);

  if (!generate_draft_data) {
    int page_count = -1;
    success = args->GetInteger(2, &page_count);
    DCHECK(success);

    if (page_count != -1) {
      bool preview_modifiable = false;
      success = settings->GetBoolean(printing::kSettingPreviewModifiable,
                                     &preview_modifiable);
      DCHECK(success);

      if (preview_modifiable &&
          print_preview_ui()->GetAvailableDraftPageCount() != page_count) {
        settings->SetBoolean(printing::kSettingGenerateDraftData, true);
      }
    }
  }

  VLOG(1) << "Print preview request start";

  rfh->Send(new PrintMsg_PrintPreview(rfh->GetRoutingID(), *settings));
}

void PrintPreviewHandler::HandlePrint(const base::ListValue* args) {
  ReportStats();

  // Record the number of times the user requests to regenerate preview data
  // before printing.
  UMA_HISTOGRAM_COUNTS("PrintPreview.RegeneratePreviewRequest.BeforePrint",
                       regenerate_preview_request_count_);
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  std::string json_str;
  CHECK(args->GetString(1, &json_str));

  std::unique_ptr<base::DictionaryValue> settings =
      GetSettingsDictionary(json_str);
  if (!settings) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value(-1));
    return;
  }

  ReportPrintSettingsStats(*settings);

  // Report whether the user printed a PDF or an HTML document.
  ReportPrintDocumentTypeHistogram(print_preview_ui()->source_is_modifiable() ?
                                   HTML_DOCUMENT : PDF_DOCUMENT);

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
    DCHECK(pdf_callback_id_.empty());
    pdf_callback_id_ = callback_id;
    PrintToPdf();
    return;
  }

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  if (print_with_privet) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintWithPrivet", page_count);
    ReportUserActionHistogram(PRINT_WITH_PRIVET);
  }
#endif
  if (print_with_extension) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintWithExtension",
                         page_count);
    ReportUserActionHistogram(PRINT_WITH_EXTENSION);
  }

  scoped_refptr<base::RefCountedBytes> data;
  base::string16 title;
  if (!GetPreviewDataAndTitle(&data, &title)) {
    // Nothing to print, no preview available.
    RejectJavascriptCallback(
        base::Value(callback_id),
        print_with_privet ? base::Value(-1) : base::Value("NO_DATA"));
    return;
  }

  if (print_with_privet || print_with_extension) {
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
      RejectJavascriptCallback(
          base::Value(callback_id),
          print_with_privet ? base::Value(-1) : base::Value("FAILED"));
      return;
    }

    PrinterHandler* handler =
        GetPrinterHandler(print_with_privet ? PrinterType::kPrivetPrinter
                                            : PrinterType::kExtensionPrinter);
    handler->StartPrint(destination_id, capabilities, title, print_ticket,
                        gfx::Size(width, height), data,
                        base::Bind(&PrintPreviewHandler::OnPrintResult,
                                   weak_factory_.GetWeakPtr(), callback_id));
    return;
  }

  if (is_cloud_printer) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToCloudPrint",
                         page_count);
    ReportUserActionHistogram(PRINT_WITH_CLOUD_PRINT);
    SendCloudPrintJob(callback_id, data.get());
    return;
  }

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  bool system_dialog = false;
  settings->GetBoolean(printing::kSettingShowSystemDialog, &system_dialog);
  if (system_dialog) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.SystemDialog", page_count);
    ReportUserActionHistogram(FALLBACK_TO_ADVANCED_SETTINGS_DIALOG);
  } else {
    UMA_HISTOGRAM_COUNTS("PrintPreview.PageCount.PrintToPrinter", page_count);
    ReportUserActionHistogram(PRINT_TO_PRINTER);
  }

  WebContents* initiator = GetInitiator();
  if (initiator) {
    // Save initiator IDs. PrintMsg_PrintForPrintPreview below should cause
    // the renderer to send PrintHostMsg_UpdatePrintSettings and trigger
    // PrintingMessageFilter::OnUpdatePrintSettings(), which needs this info.
    auto* main_render_frame = initiator->GetMainFrame();
    settings->SetInteger(printing::kPreviewInitiatorHostId,
                         main_render_frame->GetProcess()->GetID());
    settings->SetInteger(printing::kPreviewInitiatorRoutingId,
                         main_render_frame->GetRoutingID());
  }

  // Set ID to know whether printing is for preview.
  settings->SetInteger(printing::kPreviewUIID,
                       print_preview_ui()->GetIDForPrintPreviewUI());

  // Save the settings and notify print preview. Print preview will respond
  // with a "hidePreviewDialog" message, and then the message can be sent to
  // the renderer in HandleHidePreview().
  settings_ = std::move(settings);
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());

#else
  NOTREACHED();
#endif   // BUILDFLAG(ENABLE_BASIC_PRINTING)
}

void PrintPreviewHandler::PrintToPdf() {
  if (!print_to_pdf_path_.empty()) {
    // User has already selected a path, no need to show the dialog again.
    ResolveJavascriptCallback(base::Value(pdf_callback_id_), base::Value());
    pdf_callback_id_.clear();
    PostPrintToPdfTask();
  } else if (!select_file_dialog_.get() ||
             !select_file_dialog_->IsRunning(platform_util::GetTopLevel(
                 preview_web_contents()->GetNativeView()))) {
    // Pre-populating select file dialog with print job title.
    const base::string16& print_job_title_utf16 =
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
  if (settings_) {
    // Print preview is responding to a resolution of "print" promise. Send the
    // print message to the renderer.
    RenderFrameHost* rfh = preview_web_contents()->GetMainFrame();
    rfh->Send(
        new PrintMsg_PrintForPrintPreview(rfh->GetRoutingID(), *settings_));
    settings_.reset();

    // Clear the initiator so that it can open a new print preview dialog, while
    // the current print preview dialog is still handling its print job.
    WebContents* initiator = GetInitiator();
    ClearInitiatorDetails();

    // Since the preview dialog is hidden and not closed, we need to make this
    // call.
    if (initiator) {
      auto* print_view_manager = PrintViewManager::FromWebContents(initiator);
      print_view_manager->PrintPreviewDone();
    }
  }
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
  std::string callback_id;
  std::string printer_name;
  if (!args->GetString(0, &callback_id) || !args->GetString(1, &printer_name) ||
      callback_id.empty() || printer_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  if (printer_name == kLocalPdfPrinterId) {
    auto printer_info = base::MakeUnique<base::DictionaryValue>();
    printer_info->SetString(printing::kPrinterId, printer_name);
    printer_info->Set(
        printing::kPrinterCapabilities,
        GetPdfCapabilities(g_browser_process->GetApplicationLocale()));
    SendPrinterCapabilities(callback_id, printer_name, std::move(printer_info));
    return;
  }

  printing::PrinterSetupCallback cb =
      base::Bind(&PrintPreviewHandler::SendPrinterCapabilities,
                 weak_factory_.GetWeakPtr(), callback_id, printer_name);

  printer_backend_proxy()->ConfigurePrinterAndFetchCapabilities(printer_name,
                                                                cb);
}

// |args| is expected to contain a string with representing the callback id
// followed by a list of arguments the first of which should be the printer id.
void PrintPreviewHandler::HandlePrinterSetup(const base::ListValue* args) {
  std::string callback_id;
  std::string printer_name;
  if (!args->GetString(0, &callback_id) || !args->GetString(1, &printer_name) ||
      callback_id.empty() || printer_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(printer_name));
    return;
  }

  printer_backend_proxy()->ConfigurePrinterAndFetchCapabilities(
      printer_name,
      base::Bind(&PrintPreviewHandler::SendPrinterSetup,
                 weak_factory_.GetWeakPtr(), callback_id, printer_name));
}

void PrintPreviewHandler::OnSigninComplete(const std::string& callback_id) {
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
}

void PrintPreviewHandler::HandleSignin(const base::ListValue* args) {
  std::string callback_id;
  bool add_account = false;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());
  CHECK(args->GetBoolean(1, &add_account));

  Profile* profile = Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  print_dialog_cloud::CreateCloudPrintSigninTab(
      displayer.browser(), add_account,
      base::Bind(&PrintPreviewHandler::OnSigninComplete,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void PrintPreviewHandler::HandleGetAccessToken(const base::ListValue* args) {
  std::string callback_id;
  std::string type;

  bool ok = args->GetString(0, &callback_id) && args->GetString(1, &type) &&
            !callback_id.empty();
  DCHECK(ok);

  if (!token_service_)
    token_service_ = base::MakeUnique<AccessTokenService>(this);
  token_service_->RequestToken(type, callback_id);
}

void PrintPreviewHandler::HandleManageCloudPrint(
    const base::ListValue* args) {
  ++manage_cloud_printers_dialog_request_count_;
  GURL manage_url(cloud_devices::GetCloudPrintRelativeURL("manage.html"));
  std::string user;
  if (!args->GetString(0, &user))
    return;
  if (!user.empty())
    manage_url = net::AppendQueryParameter(manage_url, "authuser", user);
  preview_web_contents()->OpenURL(
      content::OpenURLParams(manage_url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
void PrintPreviewHandler::HandleShowSystemDialog(
    const base::ListValue* /*args*/) {
  ReportStats();
  ReportUserActionHistogram(FALLBACK_TO_ADVANCED_SETTINGS_DIALOG);

  WebContents* initiator = GetInitiator();
  if (!initiator)
    return;

  auto* print_view_manager = PrintViewManager::FromWebContents(initiator);
  print_view_manager->PrintForSystemDialogNow(
      base::Bind(&PrintPreviewHandler::ClosePreviewDialog,
                 weak_factory_.GetWeakPtr()));

  // Cancel the pending preview request if exists.
  print_preview_ui()->OnCancelPendingPreviewRequest();
}
#endif

void PrintPreviewHandler::HandleManagePrinters(
    const base::ListValue* /*args*/) {
  ++manage_printers_dialog_request_count_;
#if defined(OS_CHROMEOS)
  GURL local_printers_manage_url(chrome::kChromeUIMdCupsSettingsURL);
  preview_web_contents()->OpenURL(
      content::OpenURLParams(local_printers_manage_url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
#else
  printing::PrinterManagerDialog::ShowPrinterManagerDialog();
#endif
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
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  CHECK(!callback_id.empty());

  AllowJavascript();

  // Send before SendInitialSettings() to allow cloud printer auto select.
  SendCloudPrintEnabled();
  printer_backend_proxy()->GetDefaultPrinter(
      base::Bind(&PrintPreviewHandler::SendInitialSettings,
                 weak_factory_.GetWeakPtr(), callback_id));
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
    const std::string& callback_id,
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
  if (prefs) {
    const std::string rules_str =
        prefs->GetString(prefs::kPrintPreviewDefaultDestinationSelectionRules);
    if (!rules_str.empty())
      initial_settings.SetString(kDefaultDestinationSelectionRules, rules_str);
  }

  if (print_preview_ui()->source_is_modifiable())
    GetNumberFormatAndMeasurementSystem(&initial_settings);
  ResolveJavascriptCallback(base::Value(callback_id), initial_settings);
}

void PrintPreviewHandler::ClosePreviewDialog() {
  print_preview_ui()->OnClosePrintPreviewDialog();
}

void PrintPreviewHandler::SendAccessToken(const std::string& callback_id,
                                          const std::string& access_token) {
  VLOG(1) << "Get getAccessToken finished";
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(access_token));
}

void PrintPreviewHandler::SendPrinterCapabilities(
    const std::string& callback_id,
    const std::string& printer_name,
    std::unique_ptr<base::DictionaryValue> settings_info) {
  // Check that |settings_info| is valid.
  if (settings_info && settings_info->Get("capabilities", nullptr)) {
    VLOG(1) << "Get printer capabilities finished";
    ResolveJavascriptCallback(base::Value(callback_id), *settings_info);
    return;
  }

  VLOG(1) << "Get printer capabilities failed";
  RejectJavascriptCallback(base::Value(callback_id), base::Value());
}

void PrintPreviewHandler::SendPrinterSetup(
    const std::string& callback_id,
    const std::string& printer_name,
    std::unique_ptr<base::DictionaryValue> destination_info) {
  auto response = base::MakeUnique<base::DictionaryValue>();
  bool success = true;
  auto caps_value = base::MakeUnique<base::Value>();
  auto caps = base::MakeUnique<base::DictionaryValue>();
  if (destination_info &&
      destination_info->Remove(printing::kPrinterCapabilities, &caps_value) &&
      caps_value->IsType(base::Value::Type::DICTIONARY)) {
    caps = base::DictionaryValue::From(std::move(caps_value));
  } else {
    LOG(WARNING) << "Printer setup failed";
    success = false;
  }

  response->SetString("printerId", printer_name);
  response->SetBoolean("success", success);
  response->Set("capabilities", std::move(caps));

  ResolveJavascriptCallback(base::Value(callback_id), *response);
}

void PrintPreviewHandler::SetupPrinterList(
    const std::string& callback_id,
    const printing::PrinterList& printer_list) {
  base::ListValue printers;
  PrintersToValues(printer_list, &printers);

  VLOG(1) << "Enumerate printers finished, found " << printers.GetSize()
          << " printers";

  if (!has_logged_printers_count_) {
    UMA_HISTOGRAM_COUNTS("PrintPreview.NumberOfPrinters", printers.GetSize());
    has_logged_printers_count_ = true;
  }

  ResolveJavascriptCallback(base::Value(callback_id), printers);
}

void PrintPreviewHandler::SendCloudPrintEnabled() {
  Profile* profile = Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  if (prefs->GetBoolean(prefs::kCloudPrintSubmitEnabled)) {
    FireWebUIListener(
        "use-cloud-print",
        base::Value(GURL(cloud_devices::GetCloudPrintURL()).spec()),
        base::Value(chrome::IsRunningInForcedAppMode()));
  }
}

void PrintPreviewHandler::SendCloudPrintJob(const std::string& callback_id,
                                            const base::RefCountedBytes* data) {
  // BASE64 encode the job data.
  const base::StringPiece raw_data(reinterpret_cast<const char*>(data->front()),
                                   data->size());
  std::string base64_data;
  base::Base64Encode(raw_data, &base64_data);

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(base64_data));
}

WebContents* PrintPreviewHandler::GetInitiator() const {
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return NULL;
  return dialog_controller->GetInitiator(preview_web_contents());
}

void PrintPreviewHandler::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  FireWebUIListener("reload-printer-list");
}

void PrintPreviewHandler::SelectFile(const base::FilePath& default_filename,
                                     bool prompt_user) {
  if (prompt_user) {
    ChromeSelectFilePolicy policy(GetInitiator());
    if (!policy.CanOpenSelectFileDialog()) {
      policy.SelectFileDenied();
      RejectJavascriptCallback(base::Value(pdf_callback_id_), base::Value());
      pdf_callback_id_.clear();
      return;
    }
  }

  // Get save location from Download Preferences.
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  base::FilePath path = download_prefs->SaveFilePath();
  printing::StickySettings* sticky_settings = GetStickySettings();
  sticky_settings->SaveInPrefs(Profile::FromBrowserContext(
      preview_web_contents()->GetBrowserContext())->GetPrefs());

  // Handle the no prompting case. Like the dialog prompt, this function
  // returns and eventually FileSelected() gets called.
  if (!prompt_user) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(&GetUniquePath, path.Append(default_filename)),
        base::Bind(&PrintPreviewHandler::OnGotUniqueFileName,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  // If the directory is empty there is no reason to create it.
  if (path.empty()) {
    OnDirectoryCreated(default_filename);
    return;
  }

  // Create the directory to save in if it does not exist.
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&CreateDirectoryIfNeeded, path),
      base::Bind(&PrintPreviewHandler::OnDirectoryCreated,
                 weak_factory_.GetWeakPtr(), path.Append(default_filename)));
}

void PrintPreviewHandler::OnDirectoryCreated(const base::FilePath& path) {
  // Prompts the user to select the file.
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pdf"));
  file_type_info.include_all_files = true;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::NATIVE_OR_DRIVE_PATH;

  select_file_dialog_ =
      ui::SelectFileDialog::Create(this, nullptr /*policy already checked*/);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(), path,
      &file_type_info, 0, base::FilePath::StringType(),
      platform_util::GetTopLevel(preview_web_contents()->GetNativeView()),
      NULL);
}

void PrintPreviewHandler::OnGotUniqueFileName(const base::FilePath& path) {
  FileSelected(path, 0, nullptr);
}

void PrintPreviewHandler::OnPrintPreviewReady(int preview_uid, int request_id) {
  if (request_id < 0 || preview_callbacks_.empty() || !IsJavascriptAllowed()) {
    // invalid ID or extra message
    BadMessageReceived();
    return;
  }

  ResolveJavascriptCallback(base::Value(preview_callbacks_.front()),
                            base::Value(preview_uid));
  preview_callbacks_.pop();
}

void PrintPreviewHandler::OnPrintPreviewFailed() {
  if (preview_callbacks_.empty() || !IsJavascriptAllowed()) {
    BadMessageReceived();
    return;
  }

  if (!reported_failed_preview_) {
    reported_failed_preview_ = true;
    ReportUserActionHistogram(PREVIEW_FAILED);
  }
  RejectJavascriptCallback(base::Value(preview_callbacks_.front()),
                           base::Value("PREVIEW_FAILED"));
  preview_callbacks_.pop();
}

void PrintPreviewHandler::OnInvalidPrinterSettings() {
  if (preview_callbacks_.empty() || !IsJavascriptAllowed()) {
    BadMessageReceived();
    return;
  }

  RejectJavascriptCallback(base::Value(preview_callbacks_.front()),
                           base::Value("SETTINGS_INVALID"));
  preview_callbacks_.pop();
}

void PrintPreviewHandler::SendPrintPresetOptions(bool disable_scaling,
                                                 int copies,
                                                 int duplex) {
  if (preview_callbacks_.empty() || !IsJavascriptAllowed()) {
    BadMessageReceived();
    return;
  }

  FireWebUIListener("print-preset-options", base::Value(disable_scaling),
                    base::Value(copies), base::Value(duplex));
}

void PrintPreviewHandler::SendPageCountReady(int page_count,
                                             int request_id,
                                             int fit_to_page_scaling) {
  if (preview_callbacks_.empty() || !IsJavascriptAllowed()) {
    BadMessageReceived();
    return;
  }

  FireWebUIListener("page-count-ready", base::Value(page_count),
                    base::Value(request_id), base::Value(fit_to_page_scaling));
}

void PrintPreviewHandler::SendPageLayoutReady(
    const base::DictionaryValue& layout,
    bool has_custom_page_size_style) {
  if (preview_callbacks_.empty() || !IsJavascriptAllowed()) {
    BadMessageReceived();
    return;
  }

  FireWebUIListener("page-layout-ready", layout,
                    base::Value(has_custom_page_size_style));
}

void PrintPreviewHandler::SendPagePreviewReady(int page_index,
                                               int preview_uid,
                                               int preview_response_id) {
  if (!IsJavascriptAllowed()) {
    BadMessageReceived();
    return;
  }

  FireWebUIListener("page-preview-ready", base::Value(page_index),
                    base::Value(preview_uid), base::Value(preview_response_id));
}

void PrintPreviewHandler::OnPrintPreviewCancelled() {
  if (preview_callbacks_.empty() || !IsJavascriptAllowed()) {
    BadMessageReceived();
    return;
  }

  RejectJavascriptCallback(base::Value(preview_callbacks_.front()),
                           base::Value("CANCELLED"));
  preview_callbacks_.pop();
}

void PrintPreviewHandler::OnPrintRequestCancelled() {
  HandleCancelPendingPrintRequest(nullptr);
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
void PrintPreviewHandler::ShowSystemDialog() {
  HandleShowSystemDialog(NULL);
}
#endif

void PrintPreviewHandler::FileSelected(const base::FilePath& path,
                                       int /* index */,
                                       void* /* params */) {
  // Update downloads location and save sticky settings.
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
      preview_web_contents()->GetBrowserContext());
  download_prefs->SetSaveFilePath(path.DirName());
  printing::StickySettings* sticky_settings = GetStickySettings();
  sticky_settings->SaveInPrefs(
      Profile::FromBrowserContext(preview_web_contents()->GetBrowserContext())
          ->GetPrefs());
  ResolveJavascriptCallback(base::Value(pdf_callback_id_), base::Value());
  pdf_callback_id_.clear();
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

  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&PrintToPdfCallback, data, print_to_pdf_path_,
                     pdf_file_saved_closure_));
  print_to_pdf_path_.clear();
}

void PrintPreviewHandler::FileSelectionCanceled(void* params) {
  RejectJavascriptCallback(base::Value(pdf_callback_id_), base::Value());
  pdf_callback_id_.clear();
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
  DCHECK(tmp_data->size());
  DCHECK(tmp_data->front());

  *data = tmp_data;
  *title = print_preview_ui()->initiator_title();
  return true;
}

PrinterHandler* PrintPreviewHandler::GetPrinterHandler(
    PrinterType printer_type) {
  if (printer_type == PrinterType::kExtensionPrinter) {
    if (!extension_printer_handler_) {
      extension_printer_handler_ = PrinterHandler::CreateForExtensionPrinters(
          Profile::FromWebUI(web_ui()));
    }
    return extension_printer_handler_.get();
  }
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  if (printer_type == PrinterType::kPrivetPrinter) {
    if (!privet_printer_handler_) {
      privet_printer_handler_ =
          PrinterHandler::CreateForPrivetPrinters(Profile::FromWebUI(web_ui()));
    }
    return privet_printer_handler_.get();
  }
#endif
  return nullptr;
}

void PrintPreviewHandler::OnGotPrintersForExtensionOrPrivet(
    const std::string& callback_id,
    printing::PrinterType printer_type,
    const base::ListValue& printers,
    bool done) {
  DCHECK(printer_type == PrinterType::kExtensionPrinter ||
         printer_type == PrinterType::kPrivetPrinter);
  if (!printers.empty()) {
    if (printer_type == PrinterType::kExtensionPrinter) {
      FireWebUIListener("extension-printers-added", printers);
    } else {
      const base::Value* printer;
      CHECK(printers.Get(0, &printer));
      FireWebUIListener("privet-printer-added", *printer);
    }
  }
  if (done) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
  }
}

void PrintPreviewHandler::OnGotExtensionPrinterInfo(
    const std::string& callback_id,
    const base::DictionaryValue& printer_info) {
  if (printer_info.empty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  ResolveJavascriptCallback(base::Value(callback_id), printer_info);
}

void PrintPreviewHandler::OnGotPrinterCapabilities(
    const std::string& callback_id,
    const base::DictionaryValue& capabilities) {
  if (capabilities.empty()) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  ResolveJavascriptCallback(base::Value(callback_id), capabilities);
}

void PrintPreviewHandler::OnPrintResult(const std::string& callback_id,
                                        bool success,
                                        const base::Value& error) {
  if (success) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }
  RejectJavascriptCallback(base::Value(callback_id), error);
}

void PrintPreviewHandler::RegisterForGaiaCookieChanges() {
  DCHECK(!gaia_cookie_manager_service_);
  Profile* profile = Profile::FromWebUI(web_ui());
  if (signin::IsAccountConsistencyMirrorEnabled() &&
      !profile->IsOffTheRecord()) {
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

void PrintPreviewHandler::BadMessageReceived() {
  bad_message::ReceivedBadMessage(
      GetInitiator()->GetRenderProcessHost(),
      bad_message::BadMessageReason::PPH_EXTRA_PREVIEW_MESSAGE);
}

void PrintPreviewHandler::SetPdfSavedClosureForTesting(
    const base::Closure& closure) {
  pdf_file_saved_closure_ = closure;
}

void PrintPreviewHandler::SendEnableManipulateSettingsForTest() {
  FireWebUIListener("enable-manipulate-settings-for-test", base::Value());
}

void PrintPreviewHandler::SendManipulateSettingsForTest(
    const base::DictionaryValue& settings) {
  FireWebUIListener("manipulate-settings-for-test", settings);
}
