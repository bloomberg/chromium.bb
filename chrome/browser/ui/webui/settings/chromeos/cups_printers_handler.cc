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
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/fake_printer_discoverer.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printers_manager_factory.h"
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

void OnRemovedPrinter(bool success) {}

std::unique_ptr<base::DictionaryValue> GetPrinterInfo(const Printer& printer) {
  std::unique_ptr<base::DictionaryValue> printer_info =
      base::MakeUnique<base::DictionaryValue>();
  printer_info->SetString("printerId", printer.id());
  printer_info->SetString("printerName", printer.display_name());
  printer_info->SetString("printerDescription", printer.description());
  printer_info->SetString("printerManufacturer", printer.manufacturer());
  printer_info->SetString("printerModel", printer.model());

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

  printer_info->SetString("printerAddress", host);
  printer_info->SetString("printerProtocol", base::ToLowerASCII(scheme));
  if (base::ToLowerASCII(scheme) == "lpd" && !path.empty())
    printer_info->SetString("printerQueue", path.substr(1));

  return printer_info;
}

}  // namespace

CupsPrintersHandler::CupsPrintersHandler(content::WebUI* webui)
    : printer_discoverer_(nullptr),
      profile_(Profile::FromWebUI(webui)),
      weak_factory_(this) {
  ppd_provider_ = printing::CreateProvider(profile_);
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

  std::vector<std::unique_ptr<Printer>> printers =
      PrintersManagerFactory::GetForBrowserContext(profile_)->GetPrinters();

  base::ListValue* printers_list = new base::ListValue;
  for (const std::unique_ptr<Printer>& printer : printers) {
    std::unique_ptr<base::DictionaryValue> printer_info =
        GetPrinterInfo(*printer.get());
    printers_list->Append(std::move(printer_info));
  }

  std::unique_ptr<base::DictionaryValue> response =
      base::MakeUnique<base::DictionaryValue>();
  response->Set("printerList", printers_list);
  ResolveJavascriptCallback(base::StringValue(callback_id), *response);
}

void CupsPrintersHandler::HandleUpdateCupsPrinter(const base::ListValue* args) {
  std::string printer_id;
  std::string printer_name;
  CHECK(args->GetString(0, &printer_id));
  CHECK(args->GetString(1, &printer_name));

  std::unique_ptr<Printer> printer = base::MakeUnique<Printer>(printer_id);
  printer->set_display_name(printer_name);
  PrintersManagerFactory::GetForBrowserContext(profile_)->RegisterPrinter(
      std::move(printer));
}

void CupsPrintersHandler::HandleRemoveCupsPrinter(const base::ListValue* args) {
  std::string printer_id;
  std::string printer_name;
  CHECK(args->GetString(0, &printer_id));
  CHECK(args->GetString(1, &printer_name));
  PrintersManagerFactory::GetForBrowserContext(profile_)->RemovePrinter(
      printer_id);

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->CupsRemovePrinter(printer_name, base::Bind(&OnRemovedPrinter),
                            base::Bind(&base::DoNothing));
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
  std::string printer_address;
  std::string printer_protocol;
  std::string printer_queue;
  std::string printer_ppd_path;
  CHECK(printer_dict->GetString("printerId", &printer_id));
  CHECK(printer_dict->GetString("printerName", &printer_name));
  CHECK(printer_dict->GetString("printerDescription", &printer_description));
  CHECK(printer_dict->GetString("printerManufacturer", &printer_manufacturer));
  CHECK(printer_dict->GetString("printerModel", &printer_model));
  CHECK(printer_dict->GetString("printerAddress", &printer_address));
  CHECK(printer_dict->GetString("printerProtocol", &printer_protocol));
  // printerQueue might be null for a printer whose protocol is not 'LPD'.
  printer_dict->GetString("printerQueue", &printer_queue);

  // printerPPDPath might be null for an auto-discovered printer.
  printer_dict->GetString("printerPPDPath", &printer_ppd_path);
  std::string printer_uri =
      printer_protocol + "://" + printer_address + "/" + printer_queue;

  std::unique_ptr<Printer> printer = base::MakeUnique<Printer>(printer_id);
  printer->set_display_name(printer_name);
  printer->set_description(printer_description);
  printer->set_manufacturer(printer_manufacturer);
  printer->set_model(printer_model);
  printer->set_uri(printer_uri);
  if (!printer_ppd_path.empty()) {
    GURL tmp = net::FilePathToFileURL(base::FilePath(printer_ppd_path));
    if (!tmp.is_valid()) {
      LOG(ERROR) << "Invalid ppd path: " << printer_ppd_path;
      OnAddPrinterError();
      return;
    }
    printer->mutable_ppd_reference()->user_supplied_ppd_url = tmp.spec();
  } else if (!printer_manufacturer.empty() && !printer_model.empty()) {
    // Using the manufacturer and model, get a ppd reference.
    if (!ppd_provider_->GetPpdReference(printer_manufacturer, printer_model,
                                        printer->mutable_ppd_reference())) {
      LOG(ERROR) << "Failed to get ppd reference";
      OnAddPrinterError();
      return;
    }
  }

  if (printer->IsIppEverywhere()) {
    std::string printer_id = printer->id();
    std::string printer_uri = printer->uri();

    chromeos::DebugDaemonClient* client =
        chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

    client->CupsAddAutoConfiguredPrinter(
        printer_id, printer_uri,
        base::Bind(&CupsPrintersHandler::OnAddedPrinter,
                   weak_factory_.GetWeakPtr(), base::Passed(&printer)),
        base::Bind(&CupsPrintersHandler::OnAddPrinterError,
                   weak_factory_.GetWeakPtr()));
  } else {
    // We need to save a reference to members of printer since we transfer
    // ownership in the bind call.
    const Printer::PpdReference ppd_reference = printer->ppd_reference();
    ppd_provider_->ResolvePpd(
        ppd_reference,
        base::Bind(&CupsPrintersHandler::ResolvePpdDone,
                   weak_factory_.GetWeakPtr(), base::Passed(&printer)));
  }
}

void CupsPrintersHandler::OnAddedPrinter(std::unique_ptr<Printer> printer,
                                         int32_t result_code) {
  std::string printer_name = printer->display_name();
  bool success = (result_code == 0);
  if (success) {
    PrintersManagerFactory::GetForBrowserContext(profile_)->RegisterPrinter(
        std::move(printer));
  }
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::StringValue("on-add-cups-printer"),
                         base::Value(success), base::StringValue(printer_name));
}

void CupsPrintersHandler::OnAddPrinterError() {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::StringValue("on-add-cups-printer"),
                         base::Value(false), base::StringValue(""));
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
    ResolveJavascriptCallback(base::StringValue(js_callback), response);
    return;
  }

  ppd_provider_->ResolvePrinters(
      manufacturer, base::Bind(&CupsPrintersHandler::ResolvePrintersDone,
                               weak_factory_.GetWeakPtr(), js_callback));
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
    chromeos::printing::PpdProvider::CallbackResultCode result_code,
    const std::vector<std::string>& manufacturers) {
  auto manufacturers_value = base::MakeUnique<base::ListValue>();
  if (result_code == chromeos::printing::PpdProvider::SUCCESS) {
    manufacturers_value->AppendStrings(manufacturers);
  }
  base::DictionaryValue response;
  response.SetBoolean("success",
                      result_code == chromeos::printing::PpdProvider::SUCCESS);
  response.Set("manufacturers", std::move(manufacturers_value));
  ResolveJavascriptCallback(base::StringValue(js_callback), response);
}

void CupsPrintersHandler::ResolvePrintersDone(
    const std::string& js_callback,
    chromeos::printing::PpdProvider::CallbackResultCode result_code,
    const std::vector<std::string>& printers) {
  auto printers_value = base::MakeUnique<base::ListValue>();
  if (result_code == chromeos::printing::PpdProvider::SUCCESS) {
    printers_value->AppendStrings(printers);
  }
  base::DictionaryValue response;
  response.SetBoolean("success",
                      result_code == chromeos::printing::PpdProvider::SUCCESS);
  response.Set("models", std::move(printers_value));
  ResolveJavascriptCallback(base::StringValue(js_callback), response);
}

void CupsPrintersHandler::FileSelected(const base::FilePath& path,
                                       int index,
                                       void* params) {
  DCHECK(!webui_callback_id_.empty());
  ResolveJavascriptCallback(base::StringValue(webui_callback_id_),
                            base::StringValue(path.value()));
  webui_callback_id_.clear();
}

void CupsPrintersHandler::HandleStartDiscovery(const base::ListValue* args) {
  if (!printer_discoverer_.get())
    printer_discoverer_ = chromeos::PrinterDiscoverer::Create();

  printer_discoverer_->AddObserver(this);
  if (!printer_discoverer_->StartDiscovery()) {
    CallJavascriptFunction("cr.webUIListenerCallback",
                           base::StringValue("on-printer-discovery-failed"));
    printer_discoverer_->RemoveObserver(this);
  }
}

void CupsPrintersHandler::HandleStopDiscovery(const base::ListValue* args) {
  if (printer_discoverer_.get()) {
    printer_discoverer_->RemoveObserver(this);
    printer_discoverer_->StopDiscovery();
    printer_discoverer_.reset();
  }
}

void CupsPrintersHandler::OnPrintersFound(
    const std::vector<Printer>& printers) {
  std::unique_ptr<base::ListValue> printers_list =
      base::MakeUnique<base::ListValue>();
  for (const auto& printer : printers) {
    std::unique_ptr<base::DictionaryValue> printer_info =
        GetPrinterInfo(printer);
    printers_list->Append(std::move(printer_info));
  }

  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::StringValue("on-printer-discovered"),
                         *printers_list);
}

void CupsPrintersHandler::OnDiscoveryDone() {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::StringValue("on-printer-discovery-done"));
}

void CupsPrintersHandler::ResolvePpdDone(
    std::unique_ptr<Printer> printer,
    printing::PpdProvider::CallbackResultCode result,
    const std::string& ppd_contents) {
  if (result != printing::PpdProvider::SUCCESS) {
    // TODO(skau): Add appropriate failure modes crbug.com/670068.
    LOG(ERROR) << "Error resolving";
    OnAddPrinterError();
    return;
  }

  std::string printer_id = printer->id();
  std::string printer_uri = printer->uri();

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

  client->CupsAddManuallyConfiguredPrinter(
      printer_id, printer_uri, ppd_contents,
      base::Bind(&CupsPrintersHandler::OnAddedPrinter,
                 weak_factory_.GetWeakPtr(), base::Passed(&printer)),
      base::Bind(&CupsPrintersHandler::OnAddPrinterError,
                 weak_factory_.GetWeakPtr()));
}

}  // namespace settings
}  // namespace chromeos
