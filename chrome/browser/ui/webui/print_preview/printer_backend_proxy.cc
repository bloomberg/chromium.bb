// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_backend_proxy.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/webui/print_preview/printer_capabilities.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"

namespace printing {

namespace {

PrinterList EnumeratePrintersOnBlockingPoolThread() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);

  return printer_list;
}

std::unique_ptr<base::DictionaryValue> FetchCapabilitiesOnBlockingPool(
    const std::string& device_name) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  scoped_refptr<printing::PrintBackend> print_backend(
      printing::PrintBackend::CreateInstance(nullptr));

  VLOG(1) << "Get printer capabilities start for " << device_name;

  std::unique_ptr<base::DictionaryValue> printer_info;
  if (!print_backend->IsValidPrinter(device_name)) {
    LOG(WARNING) << "Invalid printer " << device_name;
    return nullptr;
  }

  PrinterBasicInfo basic_info;
  if (!print_backend->GetPrinterBasicInfo(device_name, &basic_info)) {
    return nullptr;
  }

  return GetSettingsOnBlockingPool(device_name, basic_info);
}

}  // namespace

std::string GetDefaultPrinterOnBlockingPoolThread() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  scoped_refptr<printing::PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  std::string default_printer = print_backend->GetDefaultPrinterName();
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

void EnumeratePrinters(Profile* /* profile */,
                       const EnumeratePrintersCallback& cb) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&EnumeratePrintersOnBlockingPoolThread), cb);
}

void ConfigurePrinterAndFetchCapabilities(Profile* /* profile */,
                                          const std::string& device_name,
                                          const PrinterSetupCallback& cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      base::Bind(&FetchCapabilitiesOnBlockingPool, device_name), cb);
}

}  // namespace printing
