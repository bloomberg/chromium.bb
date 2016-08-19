// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "printing/backend/print_backend.h"
#include "url/third_party/mozilla/url_parse.h"

namespace chromeos {
namespace settings {

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
  CHECK(args->GetString(0, &printer_id));
  PrinterPrefManagerFactory::GetForBrowserContext(profile_)->RemovePrinter(
      printer_id);
}

}  // namespace settings
}  // namespace chromeos
