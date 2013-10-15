// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_system_task_proxy.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/common/crash_keys.h"
#include "printing/backend/print_backend.h"
#include "printing/print_job_constants.h"

using content::BrowserThread;

const char kPrinterId[] = "printerId";
const char kDisableColorOption[] = "disableColorOption";
const char kSetDuplexAsDefault[] = "setDuplexAsDefault";
const char kPrinterDefaultDuplexValue[] = "printerDefaultDuplexValue";

PrintSystemTaskProxy::PrintSystemTaskProxy(
    const base::WeakPtr<PrintPreviewHandler>& handler,
    printing::PrintBackend* print_backend)
    : handler_(handler),
      print_backend_(print_backend) {
}

PrintSystemTaskProxy::~PrintSystemTaskProxy() {
}

void PrintSystemTaskProxy::GetPrinterCapabilities(
    const std::string& printer_name) {
  VLOG(1) << "Get printer capabilities start for " << printer_name;
  crash_keys::ScopedPrinterInfo crash_key(
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
    LOG(WARNING) << "Failed to get capabilities for " << printer_name;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PrintSystemTaskProxy::SendFailedToGetPrinterCapabilities,
                   this, printer_name));
    return;
  }

  base::DictionaryValue settings_info;
  settings_info.SetString(kPrinterId, printer_name);
  settings_info.SetBoolean(kDisableColorOption, !info.color_changeable);
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
    base::DictionaryValue* settings_info) {
  if (handler_.get())
    handler_->SendPrinterCapabilities(*settings_info);
  delete settings_info;
}

void PrintSystemTaskProxy::SendFailedToGetPrinterCapabilities(
    const std::string& printer_name) {
  if (handler_.get())
    handler_->SendFailedToGetPrinterCapabilities(printer_name);
}
