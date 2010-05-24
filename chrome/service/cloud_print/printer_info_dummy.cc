// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is dummy implementation for all configurations where print system
// for cloud print is not available.
#if !defined(CP_PRINT_SYSTEM_AVAILABLE)

#include "chrome/service/cloud_print/printer_info.h"

#include "base/logging.h"

namespace cloud_print {

void EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  NOTIMPLEMENTED();
}

bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                               PrinterCapsAndDefaults* printer_info) {
  NOTIMPLEMENTED();
  return false;
}

bool ValidatePrintTicket(const std::string& printer_name,
                         const std::string& print_ticket_data) {
  NOTIMPLEMENTED();
  return false;
}

std::string GenerateProxyId() {
  NOTIMPLEMENTED();
  return std::string();
}

bool SpoolPrintJob(const std::string& print_ticket,
                   const FilePath& print_data_file_path,
                   const std::string& print_data_mime_type,
                   const std::string& printer_name,
                   const std::string& job_title,
                   PlatformJobId* job_id_ret) {
  NOTIMPLEMENTED();
  return false;
}

bool GetJobDetails(const std::string& printer_name,
                   PlatformJobId job_id,
                   PrintJobDetails *job_details) {
  NOTIMPLEMENTED();
  return false;
}

bool IsValidPrinter(const std::string& printer_name) {
  NOTIMPLEMENTED();
  return false;
}

PrinterChangeNotifier::PrinterChangeNotifier() : state_(NULL) {
}

PrinterChangeNotifier::~PrinterChangeNotifier() {
  StopWatching();
}

bool PrinterChangeNotifier::StartWatching(const std::string& printer_name,
                                          Delegate* delegate) {
  NOTIMPLEMENTED();
  return false;
}

bool PrinterChangeNotifier::StopWatching() {
  NOTIMPLEMENTED();
  return false;
}

bool PrinterChangeNotifier::GetCurrentPrinterInfo(
    PrinterBasicInfo* printer_info) {
  NOTIMPLEMENTED();
  return false;
}
}  // namespace cloud_print

#endif  // CP_PRINT_SYSTEM_AVAILABLE

