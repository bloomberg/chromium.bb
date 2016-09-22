// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "printing/backend/print_backend.h"
#include "url/third_party/mozilla/url_parse.h"

namespace chromeos {
namespace settings {

namespace {

void onRemovedPrinter(bool success) {}

}  // namespace

CupsPrintersHandler::CupsPrintersHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)), weak_factory_(this) {}

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
      "selectPPDFile", base::Bind(&CupsPrintersHandler::HandleSelectPPDFile,
                                  base::Unretained(this)));
}

void CupsPrintersHandler::HandleGetCupsPrintersList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::vector<std::unique_ptr<Printer>> printers =
      PrinterPrefManagerFactory::GetForBrowserContext(profile_)->GetPrinters();

  base::ListValue* printers_list = new base::ListValue;
  for (const std::unique_ptr<Printer>& printer : printers) {
    std::unique_ptr<base::DictionaryValue> printer_info =
        base::MakeUnique<base::DictionaryValue>();
    printer_info->SetString("printerId", printer->id());
    printer_info->SetString("printerName", printer->display_name());
    printer_info->SetString("printerDescription", printer->description());
    printer_info->SetString("printerManufacturer", printer->manufacturer());
    printer_info->SetString("printerModel", printer->model());

    // Get protocol, ip address and queue from the printer's URI.
    const std::string printer_uri = printer->uri();
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
  PrinterPrefManagerFactory::GetForBrowserContext(profile_)->RegisterPrinter(
      std::move(printer));
}

void CupsPrintersHandler::HandleRemoveCupsPrinter(const base::ListValue* args) {
  std::string printer_id;
  std::string printer_name;
  CHECK(args->GetString(0, &printer_id));
  CHECK(args->GetString(1, &printer_name));
  PrinterPrefManagerFactory::GetForBrowserContext(profile_)->RemovePrinter(
      printer_id);

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->CupsRemovePrinter(printer_name, base::Bind(&onRemovedPrinter),
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
  CHECK(printer_dict->GetString("printerQueue", &printer_queue));
  CHECK(printer_dict->GetString("printerPPDPath", &printer_ppd_path));
  std::string printer_uri =
      printer_protocol + "://" + printer_address + "/" + printer_queue;

  std::unique_ptr<Printer> printer = base::MakeUnique<Printer>();
  printer->set_id(printer_id);
  printer->set_display_name(printer_name);
  printer->set_description(printer_description);
  printer->set_manufacturer(printer_manufacturer);
  printer->set_model(printer_model);
  printer->set_uri(printer_uri);

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->CupsAddPrinter(
      printer_name, printer_uri,
      printer_ppd_path,
      printer_ppd_path.empty() ? true : false,       // ipp everywhere
      base::Bind(&CupsPrintersHandler::OnAddedPrinter,
                 weak_factory_.GetWeakPtr(), base::Passed(std::move(printer))),
      base::Bind(&CupsPrintersHandler::OnAddPrinterError,
                 weak_factory_.GetWeakPtr()));
}

void CupsPrintersHandler::OnAddedPrinter(std::unique_ptr<Printer> printer,
                                         bool success) {
  std::string printer_name = printer->display_name();
  if (success) {
    PrinterPrefManagerFactory::GetForBrowserContext(profile_)->RegisterPrinter(
        std::move(printer));
  }
  CallJavascriptFunction(
      "cr.webUIListenerCallback", base::StringValue("on-add-cups-printer"),
      base::FundamentalValue(success), base::StringValue(printer_name));
}

void CupsPrintersHandler::OnAddPrinterError() {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::StringValue("on-add-cups-printer"),
                         base::FundamentalValue(false), base::StringValue(""));
}

void CupsPrintersHandler::HandleSelectPPDFile(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &webui_callback_id_));

  base::FilePath downloads_path = DownloadPrefs::FromDownloadManager(
      content::BrowserContext::GetDownloadManager(profile_))->DownloadPath();

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

void CupsPrintersHandler::FileSelected(const base::FilePath& path,
                                       int index,
                                       void* params) {
  DCHECK(!webui_callback_id_.empty());
  ResolveJavascriptCallback(base::StringValue(webui_callback_id_),
                            base::StringValue(path.value()));
  webui_callback_id_.clear();
}

}  // namespace settings
}  // namespace chromeos
