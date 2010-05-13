// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINTER_INFO_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINTER_INFO_H_

#include <string>
#include <vector>

#include "base/file_path.h"

// This is the interface for platform-specific code for cloud print
namespace cloud_print {

typedef int PlatformJobId;

struct PrinterBasicInfo {
  std::string printer_name;
  std::string printer_description;
  int printer_status;
  PrinterBasicInfo() : printer_status(0) {
  }
};

typedef std::vector<PrinterBasicInfo> PrinterList;

struct PrinterCapsAndDefaults {
  std::string printer_capabilities;
  std::string caps_mime_type;
  std::string printer_defaults;
  std::string defaults_mime_type;
};

enum PrintJobStatus {
  PRINT_JOB_STATUS_INVALID,
  PRINT_JOB_STATUS_IN_PROGRESS,
  PRINT_JOB_STATUS_ERROR,
  PRINT_JOB_STATUS_COMPLETED
};

struct PrintJobDetails {
  PrintJobStatus status;
  int platform_status_flags;
  std::string status_message;
  int total_pages;
  int pages_printed;
  PrintJobDetails() : status(PRINT_JOB_STATUS_INVALID),
                      platform_status_flags(0), total_pages(0),
                      pages_printed(0) {
  }
  void Clear() {
    status = PRINT_JOB_STATUS_INVALID;
    platform_status_flags = 0;
    status_message.clear();
    total_pages = 0;
    pages_printed = 0;
  }
  bool operator ==(const PrintJobDetails& other) const {
    return (status == other.status) &&
           (platform_status_flags == other.platform_status_flags) &&
           (status_message == other.status_message) &&
           (total_pages == other.total_pages) &&
           (pages_printed == other.pages_printed);
  }
  bool operator !=(const PrintJobDetails& other) const {
    return !(*this == other);
  }
};

// Enumerates the list of installed local and network printers.
void EnumeratePrinters(PrinterList* printer_list);
// Gets the capabilities and defaults for a specific printer.
bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                               PrinterCapsAndDefaults* printer_info);
bool ValidatePrintTicket(const std::string& printer_name,
                         const std::string& print_ticket_data);
std::string GenerateProxyId();
bool SpoolPrintJob(const std::string& print_ticket,
                   const FilePath& print_data_file_path,
                   const std::string& print_data_mime_type,
                   const std::string& printer_name,
                   const std::string& job_title,
                   PlatformJobId* job_id_ret);

bool GetJobDetails(const std::string& printer_name,
                   PlatformJobId job_id,
                   PrintJobDetails *job_details);
bool IsValidPrinter(const std::string& printer_name);

// A class that watches changes to a printer or a print server.
// The set of notifications are very coarse-grained (even though the Windows
// API allows for listening to fine-grained details about a printer, this class
// does not support that level of fine-grained control.
class PrinterChangeNotifier {
 public:
  class Delegate {
    public:
      virtual void OnPrinterAdded() = 0;
      virtual void OnPrinterDeleted() = 0;
      virtual void OnPrinterChanged() = 0;
      virtual void OnJobChanged() = 0;
  };
  PrinterChangeNotifier();
  ~PrinterChangeNotifier();
  bool StartWatching(const std::string& printer_name, Delegate* delegate);
  bool StopWatching();
  bool GetCurrentPrinterInfo(PrinterBasicInfo* printer_info);
 private:
  // Internal state maintained by the PrinterChangeNotifier class.
  class NotificationState;
  NotificationState* state_;
  DISALLOW_COPY_AND_ASSIGN(PrinterChangeNotifier);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef PrinterChangeNotifier::Delegate PrinterChangeNotifierDelegate;

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINTER_INFO_H_

