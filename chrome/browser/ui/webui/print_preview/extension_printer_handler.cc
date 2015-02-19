// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/local_discovery/pwg_raster_converter.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"

using local_discovery::PWGRasterConverter;

namespace {

const char kContentTypePdf[] = "application/pdf";
const char kContentTypePWGRaster[] = "image/pwg-raster";
const char kContentTypeAll[] = "*/*";

const char kInvalidDataPrintError[] = "INVALID_DATA";

// Reads raster data from file path |raster_path| and returns it as
// RefCountedMemory. Returns NULL on error.
scoped_refptr<base::RefCountedMemory> ReadConvertedPWGRasterFileOnWorkerThread(
    const base::FilePath& raster_path) {
  int64 file_size;
  if (base::GetFileSize(raster_path, &file_size) &&
      file_size <= extensions::PrinterProviderAPI::kMaxDocumentSize) {
    std::string data;
    data.reserve(file_size);

    if (base::ReadFileToString(raster_path, &data)) {
      return scoped_refptr<base::RefCountedMemory>(
          base::RefCountedString::TakeString(&data));
    }
  } else {
    LOG(ERROR) << "Invalid raster file size.";
  }
  return scoped_refptr<base::RefCountedMemory>();
}

// Posts a task to read a file containing converted PWG raster data to the
// worker pool.
void ReadConvertedPWGRasterFile(
    const ExtensionPrinterHandler::RefCountedMemoryCallback& callback,
    bool success,
    const base::FilePath& pwg_file_path) {
  if (!success) {
    callback.Run(scoped_refptr<base::RefCountedMemory>());
    return;
  }

  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(), FROM_HERE,
      base::Bind(&ReadConvertedPWGRasterFileOnWorkerThread, pwg_file_path),
      callback);
}

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
    const gfx::Size& page_size,
    const scoped_refptr<base::RefCountedMemory>& print_data,
    const PrinterHandler::PrintCallback& callback) {
  scoped_ptr<extensions::PrinterProviderAPI::PrintJob> print_job(
      new extensions::PrinterProviderAPI::PrintJob());
  print_job->printer_id = destination_id;
  print_job->ticket_json = ticket_json;

  cloud_devices::CloudDeviceDescription printer_description;
  printer_description.InitFromString(capability);

  cloud_devices::printer::ContentTypesCapability content_types;
  content_types.LoadFrom(printer_description);

  const bool kUsePdf = content_types.Contains(kContentTypePdf) ||
                       content_types.Contains(kContentTypeAll);

  if (kUsePdf) {
    print_job->content_type = kContentTypePdf;
    DispatchPrintJob(callback, print_job.Pass(), print_data);
    return;
  }

  print_job->content_type = kContentTypePWGRaster;
  ConvertToPWGRaster(print_data, printer_description, ticket_json, page_size,
                     base::Bind(&ExtensionPrinterHandler::DispatchPrintJob,
                                weak_ptr_factory_.GetWeakPtr(), callback,
                                base::Passed(&print_job)));
}

void ExtensionPrinterHandler::ConvertToPWGRaster(
    const scoped_refptr<base::RefCountedMemory>& data,
    const cloud_devices::CloudDeviceDescription& printer_description,
    const std::string& ticket_json,
    const gfx::Size& page_size,
    const ExtensionPrinterHandler::RefCountedMemoryCallback& callback) {
  cloud_devices::CloudDeviceDescription ticket;
  if (!ticket.InitFromString(ticket_json)) {
    callback.Run(scoped_refptr<base::RefCountedMemory>());
    return;
  }

  if (!pwg_raster_converter_) {
    pwg_raster_converter_ = PWGRasterConverter::CreateDefault();
  }
  pwg_raster_converter_->Start(
      data.get(),
      PWGRasterConverter::GetConversionSettings(printer_description, page_size),
      PWGRasterConverter::GetBitmapSettings(printer_description, ticket),
      base::Bind(&ReadConvertedPWGRasterFile, callback));
}

void ExtensionPrinterHandler::DispatchPrintJob(
    const PrinterHandler::PrintCallback& callback,
    scoped_ptr<extensions::PrinterProviderAPI::PrintJob> print_job,
    const scoped_refptr<base::RefCountedMemory>& print_data) {
  if (!print_data) {
    WrapPrintCallback(callback, false, kInvalidDataPrintError);
    return;
  }

  print_job->document_bytes = print_data;

  extensions::PrinterProviderAPI::GetFactoryInstance()
      ->Get(browser_context_)
      ->DispatchPrintRequested(
          *print_job, base::Bind(&ExtensionPrinterHandler::WrapPrintCallback,
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
