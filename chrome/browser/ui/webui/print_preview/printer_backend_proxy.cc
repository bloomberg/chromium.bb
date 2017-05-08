// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_backend_proxy.h"

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

namespace printing {

namespace {

PrinterList EnumeratePrintersAsync() {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  PrinterList printer_list;
  print_backend->EnumeratePrinters(&printer_list);

  return printer_list;
}

std::unique_ptr<base::DictionaryValue> FetchCapabilitiesAsync(
    const std::string& device_name) {
  base::ThreadRestrictions::AssertIOAllowed();
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

std::string GetDefaultPrinterAsync() {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<printing::PrintBackend> print_backend(
      PrintBackend::CreateInstance(nullptr));

  std::string default_printer = print_backend->GetDefaultPrinterName();
  VLOG(1) << "Default Printer: " << default_printer;
  return default_printer;
}

// Default implementation of PrinterBackendProxy.  Makes calls directly to
// the print backend on the appropriate thread.
class PrinterBackendProxyDefault : public PrinterBackendProxy {
 public:
  PrinterBackendProxyDefault() {}

  ~PrinterBackendProxyDefault() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  void GetDefaultPrinter(const DefaultPrinterCallback& cb) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(&GetDefaultPrinterAsync), cb);
  }

  void EnumeratePrinters(const EnumeratePrintersCallback& cb) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(&EnumeratePrintersAsync), cb);
  }

  void ConfigurePrinterAndFetchCapabilities(
      const std::string& device_name,
      const PrinterSetupCallback& cb) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(&FetchCapabilitiesAsync, device_name), cb);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrinterBackendProxyDefault);
};

}  // namespace

std::unique_ptr<PrinterBackendProxy> PrinterBackendProxy::Create() {
  return base::MakeUnique<PrinterBackendProxyDefault>();
}

}  // namespace printing
