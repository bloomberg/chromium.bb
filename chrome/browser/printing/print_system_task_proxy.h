// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_
#define CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_thread.h"

class PrintPreviewHandler;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace printing {
class PrintBackend;
struct PrinterCapsAndDefaults;
}

class PrintSystemTaskProxy
    : public base::RefCountedThreadSafe<
          PrintSystemTaskProxy, content::BrowserThread::DeleteOnUIThread> {
 public:
  PrintSystemTaskProxy(const base::WeakPtr<PrintPreviewHandler>& handler,
                       printing::PrintBackend* print_backend);

  void GetPrinterCapabilities(const std::string& printer_name);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<PrintSystemTaskProxy>;

  void SendPrinterCapabilities(base::DictionaryValue* settings_info);
  void SendFailedToGetPrinterCapabilities(const std::string& printer_name);

  ~PrintSystemTaskProxy();

  base::WeakPtr<PrintPreviewHandler> handler_;

  scoped_refptr<printing::PrintBackend> print_backend_;

  DISALLOW_COPY_AND_ASSIGN(PrintSystemTaskProxy);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_
