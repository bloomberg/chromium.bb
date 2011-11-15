// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_
#define CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class DictionaryValue;
class FundamentalValue;
class StringValue;
}

namespace printing {
class PrintBackend;
}

#if defined(UNIT_TEST) && defined(USE_CUPS) && !defined(OS_MACOSX)
typedef struct cups_option_s cups_option_t;

namespace printing_internal {
// Helper function to parse the lpoptions custom settings. |num_options| and
// |options| will be updated if the custom settings for |printer_name| are
// found, otherwise nothing is done.
// NOTE: This function is declared here so it can be exposed for unit testing.
void parse_lpoptions(const FilePath& filepath, const std::string& printer_name,
    int* num_options, cups_option_t** options);
}  // namespace printing_internal

#endif

class PrintSystemTaskProxy
    : public base::RefCountedThreadSafe<
          PrintSystemTaskProxy, content::BrowserThread::DeleteOnUIThread> {
 public:
  PrintSystemTaskProxy(const base::WeakPtr<PrintPreviewHandler>& handler,
                       printing::PrintBackend* print_backend,
                       bool has_logged_printers_count);

  void GetDefaultPrinter();

  void EnumeratePrinters();

  void GetPrinterCapabilities(const std::string& printer_name);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class DeleteTask<PrintSystemTaskProxy>;

  void SendDefaultPrinter(const std::string* default_printer,
                          const std::string* cloud_print_data);
  void SetupPrinterList(base::ListValue* printers);
  void SendPrinterCapabilities(base::DictionaryValue* settings_info);

  ~PrintSystemTaskProxy();

  base::WeakPtr<PrintPreviewHandler> handler_;

  scoped_refptr<printing::PrintBackend> print_backend_;

  bool has_logged_printers_count_;

  DISALLOW_COPY_AND_ASSIGN(PrintSystemTaskProxy);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_
