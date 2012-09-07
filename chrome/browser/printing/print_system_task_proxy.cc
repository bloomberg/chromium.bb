// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_system_task_proxy.h"

#include <ctype.h>

#include <string>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/common/child_process_logging.h"
#include "printing/backend/print_backend.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

#if defined(USE_CUPS)
#include <cups/cups.h>
#include <cups/ppd.h>
#endif

using content::BrowserThread;

namespace {

const char kPrinterId[] = "printerId";
const char kDisableColorOption[] = "disableColorOption";
const char kSetDuplexAsDefault[] = "setDuplexAsDefault";
const char kPrinterColorModelForBlack[] = "printerColorModelForBlack";
const char kPrinterColorModelForColor[] = "printerColorModelForColor";
const char kPrinterDefaultDuplexValue[] = "printerDefaultDuplexValue";

}  // namespace

PrintSystemTaskProxy::PrintSystemTaskProxy(
    const base::WeakPtr<PrintPreviewHandler>& handler,
    printing::PrintBackend* print_backend,
    bool has_logged_printers_count)
    : handler_(handler),
      print_backend_(print_backend),
      has_logged_printers_count_(has_logged_printers_count) {
}

PrintSystemTaskProxy::~PrintSystemTaskProxy() {
}

void PrintSystemTaskProxy::GetDefaultPrinter() {
  VLOG(1) << "Get default printer start";

  VLOG(1) << "Get default printer finished, found: "
          << print_backend_->GetDefaultPrinterName();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::SendDefaultPrinter, this,
                 print_backend_->GetDefaultPrinterName(),
                 std::string()));
}

void PrintSystemTaskProxy::SendDefaultPrinter(
    const std::string& default_printer,
    const std::string& cloud_print_data) {
  if (handler_)
    handler_->SendInitialSettings(default_printer, cloud_print_data);
}

void PrintSystemTaskProxy::EnumeratePrinters() {
  VLOG(1) << "Enumerate printers start";
  ListValue* printers = new ListValue;
  printing::PrinterList printer_list;
  print_backend_->EnumeratePrinters(&printer_list);

  if (!has_logged_printers_count_) {
    // Record the total number of printers.
    UMA_HISTOGRAM_COUNTS("PrintPreview.NumberOfPrinters",
                         printer_list.size());
  }
  int i = 0;
  for (printing::PrinterList::iterator iter = printer_list.begin();
       iter != printer_list.end(); ++iter, ++i) {
    DictionaryValue* printer_info = new DictionaryValue;
    std::string printerName;
#if defined(OS_MACOSX)
    // On Mac, |iter->printer_description| specifies the printer name and
    // |iter->printer_name| specifies the device name / printer queue name.
    printerName = iter->printer_description;
#else
    printerName = iter->printer_name;
#endif
    printer_info->SetString(printing::kSettingPrinterName, printerName);
    printer_info->SetString(printing::kSettingDeviceName, iter->printer_name);
    VLOG(1) << "Found printer " << printerName
            << " with device name " << iter->printer_name;
    printers->Append(printer_info);
  }
  VLOG(1) << "Enumerate printers finished, found " << i << " printers";

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::SetupPrinterList, this, printers));
}

void PrintSystemTaskProxy::SetupPrinterList(ListValue* printers) {
  if (handler_)
    handler_->SetupPrinterList(*printers);
  delete printers;
}

void PrintSystemTaskProxy::GetPrinterCapabilities(
    const std::string& printer_name) {
  VLOG(1) << "Get printer capabilities start for " << printer_name;
  child_process_logging::ScopedPrinterInfoSetter prn_info(
      print_backend_->GetPrinterDriverInfo(printer_name));

  if (!print_backend_->IsValidPrinter(printer_name)) {
    // TODO(gene): Notify explicitly if printer is not valid, instead of
    // failed to get capabilities.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PrintSystemTaskProxy::SendFailedToGetPrinterCapabilities,
        this, printer_name));
    return;
  }

  printing::PrinterSemanticCapsAndDefaults info;
  if (!print_backend_->GetPrinterSemanticCapsAndDefaults(printer_name, &info)) {
    VLOG(1) << "Failed to get capabilities for " << printer_name;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PrintSystemTaskProxy::SendFailedToGetPrinterCapabilities,
        this, printer_name));
    return;
  }

  DictionaryValue settings_info;
  settings_info.SetString(kPrinterId, printer_name);
  settings_info.SetBoolean(kDisableColorOption, !info.color_capable);
  settings_info.SetBoolean(printing::kSettingSetColorAsDefault,
                           info.color_default);
  // TODO(gene): Make new capabilities format for Print Preview
  // that will suit semantic capabiltities better.
  // Refactor pld API code below
  if (info.duplex_capable) {
    settings_info.SetBoolean(kSetDuplexAsDefault,
                            info.duplex_default != printing::SIMPLEX);
    settings_info.SetInteger(kPrinterDefaultDuplexValue,
                             printing::LONG_EDGE);
  } else {
    settings_info.SetBoolean(kSetDuplexAsDefault, false);
    settings_info.SetInteger(kPrinterDefaultDuplexValue,
                             printing::UNKNOWN_DUPLEX_MODE);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrintSystemTaskProxy::SendPrinterCapabilities, this,
                 settings_info.DeepCopy()));
}

void PrintSystemTaskProxy::SendPrinterCapabilities(
    DictionaryValue* settings_info) {
  if (handler_)
    handler_->SendPrinterCapabilities(*settings_info);
  delete settings_info;
}

void PrintSystemTaskProxy::SendFailedToGetPrinterCapabilities(
    const std::string& printer_name) {
  if (handler_)
    handler_->SendFailedToGetPrinterCapabilities(printer_name);
}
