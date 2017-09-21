// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"

namespace {

printing::PrinterList EnumeratePrintersAsync() {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(nullptr));

  printing::PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);
  return printer_list;
}

std::unique_ptr<base::DictionaryValue> FetchCapabilitiesAsync(
    const std::string& device_name) {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(nullptr));

  VLOG(1) << "Get printer capabilities start for " << device_name;

  if (!print_backend->IsValidPrinter(device_name)) {
    LOG(WARNING) << "Invalid printer " << device_name;
    return nullptr;
  }

  printing::PrinterBasicInfo basic_info;
  if (!print_backend->GetPrinterBasicInfo(device_name, &basic_info)) {
    return nullptr;
  }

  return printing::GetSettingsOnBlockingPool(device_name, basic_info);
}

std::string GetDefaultPrinterAsync() {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(nullptr));

  std::string default_printer = print_backend->GetDefaultPrinterName();
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

}  // namespace

LocalPrinterHandlerDefault::LocalPrinterHandlerDefault() {}

LocalPrinterHandlerDefault::~LocalPrinterHandlerDefault() {}

void LocalPrinterHandlerDefault::Reset() {}

void LocalPrinterHandlerDefault::GetDefaultPrinter(
    const DefaultPrinterCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&GetDefaultPrinterAsync), cb);
}

void LocalPrinterHandlerDefault::StartGetPrinters(
    const AddedPrintersCallback& callback,
    const GetPrintersDoneCallback& done_callback) {
  VLOG(1) << "Enumerate printers start";
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&EnumeratePrintersAsync),
      base::Bind(&printing::ConvertPrinterListForCallback, callback,
                 done_callback));
}

void LocalPrinterHandlerDefault::StartGetCapability(
    const std::string& device_name,
    const GetCapabilityCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&FetchCapabilitiesAsync, device_name), cb);
}

void LocalPrinterHandlerDefault::StartPrint(
    const std::string& destination_id,
    const std::string& capability,
    const base::string16& job_title,
    const std::string& ticket_json,
    const gfx::Size& page_size,
    const scoped_refptr<base::RefCountedBytes>& print_data,
    const PrintCallback& callback) {
  NOTREACHED();
}
