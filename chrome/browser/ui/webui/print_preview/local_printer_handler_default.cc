// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "components/printing/browser/printer_capabilities.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"

#if defined(OS_MACOSX)
#include "components/printing/browser/features.h"
#include "components/printing/browser/printer_capabilities_mac.h"
#endif

namespace printing {

namespace {

PrinterList EnumeratePrintersAsync() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);
  return printer_list;
}

base::Value FetchCapabilitiesAsync(const std::string& device_name) {
  PrinterSemanticCapsAndDefaults::Papers additional_papers;
#if defined(OS_MACOSX)
  if (base::FeatureList::IsEnabled(features::kEnableCustomMacPaperSizes))
    additional_papers = GetMacCustomPaperSizes();
#endif

  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  VLOG(1) << "Get printer capabilities start for " << device_name;

  if (!print_backend->IsValidPrinter(device_name)) {
    LOG(WARNING) << "Invalid printer " << device_name;
    return base::Value();
  }

  PrinterBasicInfo basic_info;
  if (!print_backend->GetPrinterBasicInfo(device_name, &basic_info))
    return base::Value();

  return GetSettingsOnBlockingPool(device_name, basic_info, additional_papers,
                                   print_backend);
}

std::string GetDefaultPrinterAsync() {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  std::string default_printer = print_backend->GetDefaultPrinterName();
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

}  // namespace

LocalPrinterHandlerDefault::LocalPrinterHandlerDefault(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents) {}

LocalPrinterHandlerDefault::~LocalPrinterHandlerDefault() {}

void LocalPrinterHandlerDefault::Reset() {}

void LocalPrinterHandlerDefault::GetDefaultPrinter(DefaultPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // USER_VISIBLE because the result is displayed in the print preview dialog.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetDefaultPrinterAsync), std::move(cb));
}

void LocalPrinterHandlerDefault::StartGetPrinters(
    const AddedPrintersCallback& callback,
    GetPrintersDoneCallback done_callback) {
  VLOG(1) << "Enumerate printers start";
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // USER_VISIBLE because the result is displayed in the print preview dialog.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&EnumeratePrintersAsync),
      base::BindOnce(&ConvertPrinterListForCallback, callback,
                     std::move(done_callback)));
}

void LocalPrinterHandlerDefault::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // USER_VISIBLE because the result is displayed in the print preview dialog.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&FetchCapabilitiesAsync, device_name), std::move(cb));
}

void LocalPrinterHandlerDefault::StartPrint(
    const std::string& destination_id,
    const std::string& capability,
    const base::string16& job_title,
    const std::string& ticket_json,
    const gfx::Size& page_size,
    const scoped_refptr<base::RefCountedMemory>& print_data,
    PrintCallback callback) {
  std::unique_ptr<base::Value> job_settings =
      base::JSONReader::Read(ticket_json);
  StartLocalPrint(base::Value::FromUniquePtrValue(std::move(job_settings)),
                  print_data, preview_web_contents_, std::move(callback));
}

}  // namespace printing
