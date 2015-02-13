// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "printing/print_job_constants.h"

namespace {

const char kContentTypePdf[] = "application/pdf";
const char kContentTypeAll[] = "*/*";

const char kInvalidDataPrintError[] = "INVALID_DATA";

}  // namespace

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
    const std::string& destination_id,
    const std::string& capability,
    const std::string& ticket_json,
    const scoped_refptr<base::RefCountedMemory>& print_data,
    const PrinterHandler::PrintCallback& callback) {
  extensions::PrinterProviderAPI::PrintJob print_job;
  print_job.printer_id = destination_id;
  print_job.ticket_json = ticket_json;

  cloud_devices::CloudDeviceDescription printer_description;
  printer_description.InitFromString(capability);

  cloud_devices::printer::ContentTypesCapability content_types;
  content_types.LoadFrom(printer_description);

  const bool kUsePdf = content_types.Contains(kContentTypePdf) ||
                       content_types.Contains(kContentTypeAll);

  if (!kUsePdf) {
    // TODO(tbarzic): Convert data to PWG raster if the printer does not support
    // PDF.
    WrapPrintCallback(callback, false, kInvalidDataPrintError);
    return;
  }

  print_job.content_type = kContentTypePdf;
  print_job.document_bytes = print_data;
  extensions::PrinterProviderAPI::GetFactoryInstance()
      ->Get(browser_context_)
      ->DispatchPrintRequested(
          print_job, base::Bind(&ExtensionPrinterHandler::WrapPrintCallback,
                                weak_ptr_factory_.GetWeakPtr(), callback));
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
