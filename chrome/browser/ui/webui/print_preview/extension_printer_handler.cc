// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"

ExtensionPrinterHandler::ExtensionPrinterHandler(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), weak_ptr_factory_(this) {
}

ExtensionPrinterHandler::~ExtensionPrinterHandler() {
}

void ExtensionPrinterHandler::Reset() {
  // TODO(tbarzic): Keep track of pending request ids issued by |this| and
  // cancel them from here.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExtensionPrinterHandler::StartGetPrinters(
    const PrinterHandler::GetPrintersCallback& callback) {
  extensions::PrinterProviderAPI::GetFactoryInstance()
      ->Get(browser_context_)
      ->DispatchGetPrintersRequested(
          base::Bind(&ExtensionPrinterHandler::WrapGetPrintersCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void ExtensionPrinterHandler::StartGetCapability(
    const std::string& destination_id,
    const PrinterHandler::GetCapabilityCallback& callback) {
  extensions::PrinterProviderAPI::GetFactoryInstance()
      ->Get(browser_context_)
      ->DispatchGetCapabilityRequested(
          destination_id,
          base::Bind(&ExtensionPrinterHandler::WrapGetCapabilityCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback, destination_id));
}

void ExtensionPrinterHandler::StartPrint(
    const base::DictionaryValue& print_job_settings,
    const scoped_refptr<base::RefCountedMemory>& print_data,
    const PrinterHandler::PrintCallback& callback) {
  // TODO(tbarzic): Implement this.
  WrapPrintCallback(callback, false, "NOT_IMPLEMENTED");
}

void ExtensionPrinterHandler::WrapGetPrintersCallback(
    const PrinterHandler::GetPrintersCallback& callback,
    const base::ListValue& printers,
    bool done) {
  callback.Run(printers, done);
}

void ExtensionPrinterHandler::WrapGetCapabilityCallback(
    const PrinterHandler::GetCapabilityCallback& callback,
    const std::string& destination_id,
    const base::DictionaryValue& capability) {
  callback.Run(destination_id, capability);
}

void ExtensionPrinterHandler::WrapPrintCallback(
    const PrinterHandler::PrintCallback& callback,
    bool success,
    const std::string& status) {
  callback.Run(success, status);
}
