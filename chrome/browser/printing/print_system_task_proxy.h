// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_
#define CHROME_BROWSER_PRINTING_PRINT_SYSTEM_TASK_PROXY_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_helpers.h"
#include "build/build_config.h"
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
  friend class base::DeleteHelper<PrintSystemTaskProxy>;

#if defined(UNIT_TEST) && defined(USE_CUPS)
  FRIEND_TEST_ALL_PREFIXES(PrintSystemTaskProxyTest, DetectDuplexModeCUPS);
  FRIEND_TEST_ALL_PREFIXES(PrintSystemTaskProxyTest, DetectNoDuplexModeCUPS);

  // Only used for testing.
  PrintSystemTaskProxy();
#endif

#if defined(USE_CUPS)
  static bool GetPrinterCapabilitiesCUPS(
      const printing::PrinterCapsAndDefaults& printer_info,
      const std::string& printer_name,
      bool* set_color_as_default,
      int* printer_color_space_for_color,
      int* printer_color_space_for_black,
      bool* set_duplex_as_default,
      int* default_duplex_setting_value);
#elif defined(OS_WIN)
  void GetPrinterCapabilitiesWin(
      const printing::PrinterCapsAndDefaults& printer_info,
      bool* set_color_as_default,
      int* printer_color_space_for_color,
      int* printer_color_space_for_black,
      bool* set_duplex_as_default,
      int* default_duplex_setting_value);
#endif

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
