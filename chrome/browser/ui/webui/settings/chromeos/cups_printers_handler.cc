// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/combining_printer_detector.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_info.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/google_api_keys.h"
#include "net/base/filename_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "printing/backend/print_backend.h"
#include "url/third_party/mozilla/url_parse.h"

namespace chromeos {
namespace settings {

namespace {

const char kIppScheme[] = "ipp";
const char kIppsScheme[] = "ipps";

const int kIppPort = 631;
const int kIppsPort = 443;

// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum PpdSourceForHistogram { kUser = 0, kScs = 1, kPpdSourceMax };

void RecordPpdSource(const PpdSourceForHistogram& source) {
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PpdSource", source, kPpdSourceMax);
}

void OnRemovedPrinter(const Printer::PrinterProtocol& protocol, bool success) {
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterRemoved", protocol,
                            Printer::PrinterProtocol::kProtocolMax);
}

// Returns a JSON representation of |printer| as a CupsPrinterInfo.
std::unique_ptr<base::DictionaryValue> GetPrinterInfo(const Printer& printer) {
  std::unique_ptr<base::DictionaryValue> printer_info =
      base::MakeUnique<base::DictionaryValue>();
  printer_info->SetString("printerId", printer.id());
  printer_info->SetString("printerName", printer.display_name());
  printer_info->SetString("printerDescription", printer.description());
  printer_info->SetString("printerManufacturer", printer.manufacturer());
  printer_info->SetString("printerModel", printer.model());
  printer_info->SetString("printerMakeAndModel", printer.make_and_model());
  // Get protocol, ip address and queue from the printer's URI.
  const std::string printer_uri = printer.uri();
  url::Parsed parsed;
  url::ParseStandardURL(printer_uri.c_str(), printer_uri.length(), &parsed);

  std::string scheme;
  std::string host;
  std::string path;
  if (parsed.scheme.len > 0)
    scheme = std::string(printer_uri, parsed.scheme.begin, parsed.scheme.len);
  if (parsed.host.len > 0)
    host = std::string(printer_uri, parsed.host.begin, parsed.host.len);
  if (parsed.path.len > 0)
    path = std::string(printer_uri, parsed.path.begin, parsed.path.len);
  if (base::ToLowerASCII(scheme) == "usb") {
    // USB has URI path (and, maybe, query) components that aren't really
    // associated with a queue -- the mapping between printing semantics and URI
    // semantics breaks down a bit here.  From the user's point of view, the
    // entire host/path/query block is the printer address for USB.
    printer_info->SetString("printerAddress",
                            printer_uri.substr(parsed.host.begin));
  } else {
    printer_info->SetString("printerAddress", host);
    if (!path.empty()) {
      printer_info->SetString("printerQueue", path.substr(1));
    }
  }
  printer_info->SetString("printerProtocol", base::ToLowerASCII(scheme));

  return printer_info;
}

// Extracts a sanitized value of printerQueeu from |printer_dict|.  Returns an
// empty string if the value was not present in the dictionary.
std::string GetPrinterQueue(const base::DictionaryValue& printer_dict) {
  std::string queue;
  if (!printer_dict.GetString("printerQueue", &queue)) {
    return queue;
  }

  if (!queue.empty() && queue[0] == '/') {
    // Strip the leading backslash. It is expected that this results in an
    // empty string if the input is just a backslash.
    queue = queue.substr(1);
  }

  return queue;
}

}  // namespace

CupsPrintersHandler::CupsPrintersHandler(content::WebUI* webui)
    : printer_detector_(nullptr),
      profile_(Profile::FromWebUI(webui)),
      weak_factory_(this) {
  ppd_provider_ = CreatePpdProvider(profile_);
  printer_configurer_ = PrinterConfigurer::Create(profile_);
}

CupsPrintersHandler::~CupsPrintersHandler() {}

void CupsPrintersHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getCupsPrintersList",
      base::Bind(&CupsPrintersHandler::HandleGetCupsPrintersList,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateCupsPrinter",
      base::Bind(&CupsPrintersHandler::HandleUpdateCupsPrinter,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeCupsPrinter",
      base::Bind(&CupsPrintersHandler::HandleRemoveCupsPrinter,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addCupsPrinter", base::Bind(&CupsPrintersHandler::HandleAddCupsPrinter,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrinterInfo", base::Bind(&CupsPrintersHandler::HandleGetPrinterInfo,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCupsPrinterManufacturersList",
      base::Bind(&CupsPrintersHandler::HandleGetCupsPrinterManufacturers,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCupsPrinterModelsList",
      base::Bind(&CupsPrintersHandler::HandleGetCupsPrinterModels,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "selectPPDFile", base::Bind(&CupsPrintersHandler::HandleSelectPPDFile,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startDiscoveringPrinters",
      base::Bind(&CupsPrintersHandler::HandleStartDiscovery,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopDiscoveringPrinters",
      base::Bind(&CupsPrintersHandler::HandleStopDiscovery,
                 base::Unretained(this)));
}

void CupsPrintersHandler::HandleGetCupsPrintersList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::vector<Printer> printers =
      SyncedPrintersManagerFactory::GetForBrowserContext(profile_)
          ->GetConfiguredPrinters();

  auto printers_list = base::MakeUnique<base::ListValue>();
  for (const Printer& printer : printers) {
    printers_list->Append(GetPrinterInfo(printer));
  }

  auto response = base::MakeUnique<base::DictionaryValue>();
  response->Set("printerList", std::move(printers_list));
  ResolveJavascriptCallback(base::Value(callback_id), *response);
}

void CupsPrintersHandler::HandleUpdateCupsPrinter(const base::ListValue* args) {
  std::string printer_id;
  std::string printer_name;
  CHECK(args->GetString(0, &printer_id));
  CHECK(args->GetString(1, &printer_name));

  Printer printer(printer_id);
  printer.set_display_name(printer_name);
  SyncedPrintersManagerFactory::GetForBrowserContext(profile_)
      ->UpdateConfiguredPrinter(printer);
}

void CupsPrintersHandler::HandleRemoveCupsPrinter(const base::ListValue* args) {
  std::string printer_id;
  std::string printer_name;
  CHECK(args->GetString(0, &printer_id));
  CHECK(args->GetString(1, &printer_name));
  SyncedPrintersManager* prefs =
      SyncedPrintersManagerFactory::GetForBrowserContext(profile_);
  auto printer = prefs->GetPrinter(printer_id);
  if (!printer)
    return;

  Printer::PrinterProtocol protocol = printer->GetProtocol();
  prefs->RemoveConfiguredPrinter(printer_id);

  DebugDaemonClient* client = DBusThreadManager::Get()->GetDebugDaemonClient();
  client->CupsRemovePrinter(printer_name,
                            base::Bind(&OnRemovedPrinter, protocol),
                            base::Bind(&base::DoNothing));
}

void CupsPrintersHandler::HandleGetPrinterInfo(const base::ListValue* args) {
  DCHECK(args);
  std::string callback_id;
  if (!args->GetString(0, &callback_id)) {
    NOTREACHED() << "Expected request for a promise";
    return;
  }

  const base::DictionaryValue* printer_dict = nullptr;
  if (!args->GetDictionary(1, &printer_dict)) {
    NOTREACHED() << "Dictionary missing";
    return;
  }

  AllowJavascript();

  std::string printer_address;
  if (!printer_dict->GetString("printerAddress", &printer_address)) {
    NOTREACHED() << "Address missing";
    return;
  }

  if (printer_address.empty()) {
    // Run the failure callback.
    OnPrinterInfo(callback_id, false, "", "", "", false);
    return;
  }

  std::string printer_queue = GetPrinterQueue(*printer_dict);

  std::string printer_protocol;
  if (!printer_dict->GetString("printerProtocol", &printer_protocol)) {
    NOTREACHED() << "Protocol missing";
    return;
  }

  // Parse url to separate address and port. ParseStandardURL expects a scheme,
  // so add the printer_protocol.
  std::string printer_uri =
      printer_protocol + url::kStandardSchemeSeparator + printer_address;
  const char* uri_ptr = printer_uri.c_str();
  url::Parsed parsed;
  url::ParseStandardURL(uri_ptr, printer_uri.length(), &parsed);
  base::StringPiece host(&printer_uri[parsed.host.begin], parsed.host.len);

  int port = ParsePort(uri_ptr, parsed.port);
  if (port == url::SpecialPort::PORT_UNSPECIFIED ||
      port == url::SpecialPort::PORT_INVALID) {
    // imply port from protocol
    if (printer_protocol == kIppScheme) {
      port = kIppPort;
    } else if (printer_protocol == kIppsScheme) {
      // ipps is ipp over https so it uses the https port.
      port = kIppsPort;
    } else {
      NOTREACHED() << "Unrecognized protocol. Port was not set.";
    }
  }

  QueryIppPrinter(host.as_string(), port, printer_queue,
                  base::Bind(&CupsPrintersHandler::OnPrinterInfo,
                             weak_factory_.GetWeakPtr(), callback_id));
}

void CupsPrintersHandler::OnPrinterInfo(const std::string& callback_id,
                                        bool success,
                                        const std::string& make,
                                        const std::string& model,
                                        const std::string& make_and_model,
                                        bool ipp_everywhere) {
  if (!success) {
    base::DictionaryValue reject;
    reject.SetString("message", "Querying printer failed");
    RejectJavascriptCallback(base::Value(callback_id), reject);
    return;
  }

  base::DictionaryValue info;
  info.SetString("manufacturer", make);
  info.SetString("model", model);
  info.SetString("makeAndModel", make_and_model);
  info.SetBoolean("autoconf", ipp_everywhere);
  ResolveJavascriptCallback(base::Value(callback_id), info);
}

void CupsPrintersHandler::HandleAddCupsPrinter(const base::ListValue* args) {
  AllowJavascript();

  const base::DictionaryValue* printer_dict = nullptr;
  CHECK(args->GetDictionary(0, &printer_dict));

  std::string printer_id;
  std::string printer_name;
  std::string printer_description;
  std::string printer_manufacturer;
  std::string printer_model;
  std::string printer_make_and_model;
  std::string printer_address;
  std::string printer_protocol;
  CHECK(printer_dict->GetString("printerId", &printer_id));
  CHECK(printer_dict->GetString("printerName", &printer_name));
  CHECK(printer_dict->GetString("printerDescription", &printer_description));
  CHECK(printer_dict->GetString("printerManufacturer", &printer_manufacturer));
  CHECK(printer_dict->GetString("printerModel", &printer_model));
  CHECK(
      printer_dict->GetString("printerMakeAndModel", &printer_make_and_model));
  CHECK(printer_dict->GetString("printerAddress", &printer_address));
  CHECK(printer_dict->GetString("printerProtocol", &printer_protocol));

  std::string printer_queue = GetPrinterQueue(*printer_dict);

  std::string printer_uri =
      printer_protocol + url::kStandardSchemeSeparator + printer_address;
  if (!printer_queue.empty()) {
    printer_uri += "/" + printer_queue;
  }

  // Read PPD selection if it was used.
  std::string ppd_manufacturer;
  std::string ppd_model;
  printer_dict->GetString("ppdManufacturer", &ppd_manufacturer);
  printer_dict->GetString("ppdModel", &ppd_model);

  // Read user provided PPD if it was used.
  std::string printer_ppd_path;
  printer_dict->GetString("printerPPDPath", &printer_ppd_path);

  Printer printer(printer_id);
  printer.set_display_name(printer_name);
  printer.set_description(printer_description);
  printer.set_manufacturer(printer_manufacturer);
  printer.set_model(printer_model);
  printer.set_make_and_model(printer_make_and_model);
  printer.set_uri(printer_uri);

  bool autoconf = false;
  printer_dict->GetBoolean("printerAutoconf", &autoconf);

  // Verify that the printer is autoconf or a valid ppd path is present.
  if (autoconf) {
    printer.mutable_ppd_reference()->autoconf = true;
  } else if (!printer_ppd_path.empty()) {
    RecordPpdSource(kUser);
    GURL tmp = net::FilePathToFileURL(base::FilePath(printer_ppd_path));
    if (!tmp.is_valid()) {
      LOG(ERROR) << "Invalid ppd path: " << printer_ppd_path;
      OnAddPrinterError();
      return;
    }
    printer.mutable_ppd_reference()->user_supplied_ppd_url = tmp.spec();
  } else if (!ppd_manufacturer.empty() && !ppd_model.empty()) {
    RecordPpdSource(kScs);
    // Pull out the ppd reference associated with the selected manufacturer and
    // model.
    bool found = false;
    for (const auto& resolved_printer : resolved_printers_[ppd_manufacturer]) {
      if (resolved_printer.first == ppd_model) {
        *printer.mutable_ppd_reference() = resolved_printer.second;
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "Failed to get ppd reference";
      OnAddPrinterError();
      return;
    }

    if (printer.make_and_model().empty()) {
      // In lieu of more accurate information, populate the make and model
      // fields with the PPD information.
      printer.set_manufacturer(ppd_manufacturer);
      printer.set_model(ppd_model);
      // PPD Model names are actually make and model.
      printer.set_make_and_model(ppd_model);
    }
  } else {
    // TODO(crbug.com/738514): Support PPD guessing for non-autoconf printers.
    // i.e. !autoconf && !manufacturer.empty() && !model.empty()
    NOTREACHED()
        << "A configuration option must have been selected to add a printer";
  }

  printer_configurer_->SetUpPrinter(
      printer, base::Bind(&CupsPrintersHandler::OnAddedPrinter,
                          weak_factory_.GetWeakPtr(), printer));
}

void CupsPrintersHandler::OnAddedPrinter(const Printer& printer,
                                         PrinterSetupResult result_code) {
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterSetupResult", result_code,
                            PrinterSetupResult::kMaxValue);
  switch (result_code) {
    case PrinterSetupResult::kSuccess: {
      UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.PrinterAdded",
                                printer.GetProtocol(), Printer::kProtocolMax);
      auto* manager =
          SyncedPrintersManagerFactory::GetForBrowserContext(profile_);
      manager->PrinterInstalled(printer);
      manager->UpdateConfiguredPrinter(printer);
      break;
    }
    case PrinterSetupResult::kPpdNotFound:
      LOG(WARNING) << "Could not locate requested PPD";
      break;
    case PrinterSetupResult::kPpdTooLarge:
      LOG(WARNING) << "PPD is too large";
      break;
    case PrinterSetupResult::kPpdUnretrievable:
      LOG(WARNING) << "Could not retrieve PPD from server";
      break;
    case PrinterSetupResult::kInvalidPpd:
      LOG(WARNING) << "Provided PPD is invalid.";
      break;
    case PrinterSetupResult::kPrinterUnreachable:
      LOG(WARNING) << "Could not contact printer for configuration";
      break;
    case PrinterSetupResult::kDbusError:
    case PrinterSetupResult::kFatalError:
      LOG(ERROR) << "Unrecoverable error.  Reboot required.";
      break;
    case PrinterSetupResult::kMaxValue:
      NOTREACHED() << "This is not an expected value";
      break;
  }
  CallJavascriptFunction(
      "cr.webUIListenerCallback", base::Value("on-add-cups-printer"),
      base::Value(result_code == PrinterSetupResult::kSuccess),
      base::Value(printer.display_name()));
}

void CupsPrintersHandler::OnAddPrinterError() {
  FireWebUIListener("on-add-cups-printer", base::Value(false), base::Value(""));
}

void CupsPrintersHandler::HandleGetCupsPrinterManufacturers(
    const base::ListValue* args) {
  AllowJavascript();
  std::string js_callback;
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &js_callback));
  ppd_provider_->ResolveManufacturers(
      base::Bind(&CupsPrintersHandler::ResolveManufacturersDone,
                 weak_factory_.GetWeakPtr(), js_callback));
}

void CupsPrintersHandler::HandleGetCupsPrinterModels(
    const base::ListValue* args) {
  AllowJavascript();
  std::string js_callback;
  std::string manufacturer;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &js_callback));
  CHECK(args->GetString(1, &manufacturer));

  // Empty manufacturer queries may be triggered as a part of the ui
  // initialization, and should just return empty results.
  if (manufacturer.empty()) {
    base::DictionaryValue response;
    response.SetBoolean("success", true);
    response.Set("models", base::MakeUnique<base::ListValue>());
    ResolveJavascriptCallback(base::Value(js_callback), response);
    return;
  }

  ppd_provider_->ResolvePrinters(
      manufacturer,
      base::Bind(&CupsPrintersHandler::ResolvePrintersDone,
                 weak_factory_.GetWeakPtr(), manufacturer, js_callback));
}

void CupsPrintersHandler::HandleSelectPPDFile(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &webui_callback_id_));

  base::FilePath downloads_path =
      DownloadPrefs::FromDownloadManager(
          content::BrowserContext::GetDownloadManager(profile_))
          ->DownloadPath();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  gfx::NativeWindow owning_window =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents())
          ->window()
          ->GetNativeWindow();
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(), downloads_path,
      nullptr, 0, FILE_PATH_LITERAL(""), owning_window, nullptr);
}

void CupsPrintersHandler::ResolveManufacturersDone(
    const std::string& js_callback,
    PpdProvider::CallbackResultCode result_code,
    const std::vector<std::string>& manufacturers) {
  auto manufacturers_value = base::MakeUnique<base::ListValue>();
  if (result_code == PpdProvider::SUCCESS) {
    manufacturers_value->AppendStrings(manufacturers);
  }
  base::DictionaryValue response;
  response.SetBoolean("success", result_code == PpdProvider::SUCCESS);
  response.Set("manufacturers", std::move(manufacturers_value));
  ResolveJavascriptCallback(base::Value(js_callback), response);
}

void CupsPrintersHandler::ResolvePrintersDone(
    const std::string& manufacturer,
    const std::string& js_callback,
    PpdProvider::CallbackResultCode result_code,
    const PpdProvider::ResolvedPrintersList& printers) {
  auto printers_value = base::MakeUnique<base::ListValue>();
  if (result_code == PpdProvider::SUCCESS) {
    resolved_printers_[manufacturer] = printers;
    for (const auto& printer : printers) {
      printers_value->AppendString(printer.first);
    }
  }
  base::DictionaryValue response;
  response.SetBoolean("success", result_code == PpdProvider::SUCCESS);
  response.Set("models", std::move(printers_value));
  ResolveJavascriptCallback(base::Value(js_callback), response);
}

void CupsPrintersHandler::FileSelected(const base::FilePath& path,
                                       int index,
                                       void* params) {
  DCHECK(!webui_callback_id_.empty());
  ResolveJavascriptCallback(base::Value(webui_callback_id_),
                            base::Value(path.value()));
  webui_callback_id_.clear();
}

void CupsPrintersHandler::HandleStartDiscovery(const base::ListValue* args) {
  // Create (or recreate) the printer_detector_.  If one already existed, we
  // want to start over anyways.
  printer_detector_ = CombiningPrinterDetector::Create();
  PrinterDetector* usb_detector =
      UsbPrinterDetectorFactory::GetInstance()->Get(profile_);
  DCHECK(usb_detector);
  printer_detector_->AddDetector(usb_detector);
  printer_detector_->AddObserver(this);
  OnPrintersFound(printer_detector_->GetPrinters());
  printer_detector_->Start();
}

void CupsPrintersHandler::HandleStopDiscovery(const base::ListValue* args) {
  printer_detector_.reset();
}

void CupsPrintersHandler::OnPrintersFound(
    const std::vector<PrinterDetector::DetectedPrinter>& printers) {
  std::unique_ptr<base::ListValue> printers_list =
      base::MakeUnique<base::ListValue>();
  // Filter out already-configured printers as we go.
  SyncedPrintersManager* printers_manager =
      SyncedPrintersManagerFactory::GetForBrowserContext(profile_);
  if (printers_manager != nullptr) {
    for (const auto& detected_printer : printers) {
      if (printers_manager->GetPrinter(detected_printer.printer.id()).get() ==
          nullptr) {
        printers_list->Append(GetPrinterInfo(detected_printer.printer));
      }
    }
  } else {
    LOG(WARNING) << "Failing to get available printers because no "
                    "SyncedPrintersManager exists.";
  }
  FireWebUIListener("on-printer-discovered", *printers_list);
}

void CupsPrintersHandler::OnPrinterScanComplete() {
  UMA_HISTOGRAM_COUNTS_100("Printing.CUPS.PrintersDiscovered",
                           printer_detector_->GetPrinters().size());
  FireWebUIListener("on-printer-discovery-done");
}

}  // namespace settings
}  // namespace chromeos
