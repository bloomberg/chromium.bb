// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_WIN_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_WIN_H_

#include "chrome/service/cloud_print/print_system.h"

namespace cloud_print {

class PrintSystemWin : public PrintSystem {
 public:
  PrintSystemWin();
  virtual ~PrintSystemWin();

  // PrintSystem implementation.
  virtual PrintSystem::PrintSystemResult EnumeratePrinters(
      printing::PrinterList* printer_list) OVERRIDE;
  virtual bool IsValidPrinter(const std::string& printer_name) OVERRIDE;
  virtual bool GetJobDetails(const std::string& printer_name,
                             PlatformJobId job_id,
                             PrintJobDetails *job_details) OVERRIDE;
  virtual PrintSystem::PrintServerWatcher* CreatePrintServerWatcher() OVERRIDE;
  virtual PrintSystem::PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name) OVERRIDE;

 protected:
  std::string GetPrinterDriverInfo(const std::string& printer_name) const;

 private:
  scoped_refptr<printing::PrintBackend> print_backend_;
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_WIN_H_
