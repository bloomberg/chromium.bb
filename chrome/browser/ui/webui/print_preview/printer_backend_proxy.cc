// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/printer_backend_proxy.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
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

}  // namespace printing
