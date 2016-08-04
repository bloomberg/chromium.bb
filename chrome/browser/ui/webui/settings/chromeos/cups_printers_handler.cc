// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "printing/backend/print_backend.h"

namespace {

// TODO(xdai): Retrieve the CUPS printers list from user preference instead.
void EnumeratePrintersOnBlockingPoolThread(base::ListValue* printers) {
  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(nullptr));

  printing::PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);

  for (const printing::PrinterBasicInfo& printer : printer_list) {
    std::unique_ptr<base::DictionaryValue> printer_info(
        new base::DictionaryValue);
    printer_info->SetString("printerName", printer.printer_name);
    printer_info->SetString("printerDescription", printer.printer_description);
    for (const auto opt_it : printer.options) {
      // TODO(xdai): Get "printerAddress" and "printerProtocol" from URI.
      if (opt_it.first == "printer-make-and-model")
        printer_info->SetString("printerModel", opt_it.second);
    }
    printers->Append(std::move(printer_info));
  }
}

}  // namespace

namespace chromeos {
namespace settings {

CupsPrintersHandler::CupsPrintersHandler() : weak_factory_(this) {}

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

// TODO(xdai): Retrieve the CUPS printer list from user preference instead after
// the CL https://codereview.chromium.org/2161933003/ is landed.
void CupsPrintersHandler::HandleGetCupsPrintersList(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  base::ListValue* printers_list = new base::ListValue;
  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE, base::Bind(&EnumeratePrintersOnBlockingPoolThread,
                            base::Unretained(printers_list)),
      base::Bind(&CupsPrintersHandler::OnGetCupsPrintersList,
                 weak_factory_.GetWeakPtr(), callback_id,
                 base::Owned(printers_list)));
}

void CupsPrintersHandler::OnGetCupsPrintersList(
    const std::string callback_id,
    base::ListValue* printers_list) {
  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  response->Set("printerList", printers_list->DeepCopy());
  ResolveJavascriptCallback(base::StringValue(callback_id), *response);
}

void CupsPrintersHandler::HandleUpdateCupsPrinter(const base::ListValue* args) {
  // TODO(xdai): Implement it after https://codereview.chromium.org/2161933003/
  // is landed.
}

void CupsPrintersHandler::HandleRemoveCupsPrinter(const base::ListValue* args) {
  // TODO(xdai): Implement it after https://codereview.chromium.org/2161933003/
  // is landed.
}

}  // namespace settings
}  // namespace chromeos
