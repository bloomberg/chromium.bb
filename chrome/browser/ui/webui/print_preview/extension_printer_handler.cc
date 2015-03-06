// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task_runner_util.h"
#include "chrome/browser/local_discovery/pwg_raster_converter.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"

using local_discovery::PWGRasterConverter;

namespace {

const char kContentTypePdf[] = "application/pdf";
const char kContentTypePWGRaster[] = "image/pwg-raster";
const char kContentTypeAll[] = "*/*";

const char kInvalidDataPrintError[] = "INVALID_DATA";
const char kInvalidTicketPrintError[] = "INVALID_TICKET";

// Updates |job| with raster file path, size and last modification time.
// Returns the updated print job.
scoped_ptr<extensions::PrinterProviderPrintJob> UpdateJobFileInfoOnWorkerThread(
    const base::FilePath& raster_path,
    scoped_ptr<extensions::PrinterProviderPrintJob> job) {
  if (base::GetFileInfo(raster_path, &job->file_info))
    job->document_path = raster_path;
  return job.Pass();
}

// Callback to PWG raster conversion.
// Posts a task to update print job with info about file containing converted
// PWG raster data. The task is posted to |slow_task_runner|.
void UpdateJobFileInfo(
    scoped_ptr<extensions::PrinterProviderPrintJob> job,
    const scoped_refptr<base::TaskRunner>& slow_task_runner,
    const ExtensionPrinterHandler::PrintJobCallback& callback,
    bool success,
    const base::FilePath& pwg_file_path) {
  if (!success) {
    callback.Run(job.Pass());
    return;
  }

  base::PostTaskAndReplyWithResult(
      slow_task_runner.get(), FROM_HERE,
      base::Bind(&UpdateJobFileInfoOnWorkerThread, pwg_file_path,
                 base::Passed(&job)),
      callback);
}

}  // namespace

ExtensionPrinterHandler::ExtensionPrinterHandler(
    content::BrowserContext* browser_context,
    const scoped_refptr<base::TaskRunner>& slow_task_runner)
    : browser_context_(browser_context),
      slow_task_runner_(slow_task_runner),
      weak_ptr_factory_(this) {
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
  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(browser_context_)
      ->DispatchGetPrintersRequested(
          base::Bind(&ExtensionPrinterHandler::WrapGetPrintersCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void ExtensionPrinterHandler::StartGetCapability(
    const std::string& destination_id,
    const PrinterHandler::GetCapabilityCallback& callback) {
  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(browser_context_)
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
  scoped_ptr<extensions::PrinterProviderPrintJob> print_job(
      new extensions::PrinterProviderPrintJob());
  print_job->printer_id = destination_id;
  print_job->ticket_json = ticket_json;

  cloud_devices::CloudDeviceDescription printer_description;
  printer_description.InitFromString(capability);

  cloud_devices::printer::ContentTypesCapability content_types;
  content_types.LoadFrom(printer_description);

  const bool kUsePdf = content_types.Contains(kContentTypePdf) ||
                       content_types.Contains(kContentTypeAll);

  if (kUsePdf) {
    // TODO(tbarzic): Consider writing larger PDF to disk and provide the data
    // the same way as it's done with PWG raster.
    print_job->content_type = kContentTypePdf;
    print_job->document_bytes = print_data;
    DispatchPrintJob(callback, print_job.Pass());
    return;
  }

  cloud_devices::CloudDeviceDescription ticket;
  if (!ticket.InitFromString(ticket_json)) {
    WrapPrintCallback(callback, false, kInvalidTicketPrintError);
    return;
  }

  print_job->content_type = kContentTypePWGRaster;
  ConvertToPWGRaster(print_data, printer_description, ticket, page_size,
                     print_job.Pass(),
                     base::Bind(&ExtensionPrinterHandler::DispatchPrintJob,
                                weak_ptr_factory_.GetWeakPtr(), callback));
}

void ExtensionPrinterHandler::SetPwgRasterConverterForTesting(
    scoped_ptr<local_discovery::PWGRasterConverter> pwg_raster_converter) {
  pwg_raster_converter_ = pwg_raster_converter.Pass();
}

void ExtensionPrinterHandler::ConvertToPWGRaster(
    const scoped_refptr<base::RefCountedMemory>& data,
    const cloud_devices::CloudDeviceDescription& printer_description,
    const cloud_devices::CloudDeviceDescription& ticket,
    const gfx::Size& page_size,
    scoped_ptr<extensions::PrinterProviderPrintJob> job,
    const ExtensionPrinterHandler::PrintJobCallback& callback) {
  if (!pwg_raster_converter_) {
    pwg_raster_converter_ = PWGRasterConverter::CreateDefault();
  }
  pwg_raster_converter_->Start(
      data.get(),
      PWGRasterConverter::GetConversionSettings(printer_description, page_size),
      PWGRasterConverter::GetBitmapSettings(printer_description, ticket),
      base::Bind(&UpdateJobFileInfo, base::Passed(&job), slow_task_runner_,
                 callback));
}

void ExtensionPrinterHandler::DispatchPrintJob(
    const PrinterHandler::PrintCallback& callback,
    scoped_ptr<extensions::PrinterProviderPrintJob> print_job) {
  if (print_job->document_path.empty() && !print_job->document_bytes) {
    WrapPrintCallback(callback, false, kInvalidDataPrintError);
    return;
  }

  extensions::PrinterProviderAPIFactory::GetInstance()
      ->GetForBrowserContext(browser_context_)
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
