// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/values.h"

// This is the interface for platform-specific code for cloud print
namespace cloud_print {

typedef int PlatformJobId;

struct PrinterBasicInfo {
  std::string printer_name;
  std::string printer_description;
  int printer_status;
  std::map<std::string, std::string> options;
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

// PrintSystem class will provide interface for different printing systems
// (Windows, CUPS) to implement. User will call CreateInstance() to
// obtain available printing system.
// Please note, that PrintSystem is not platform specific, but rather
// print system specific. For example, CUPS is available on both Linux and Mac,
// but not avaialble on ChromeOS, etc. This design allows us to add more
// functionality on some platforms, while reusing core (CUPS) functions.
class PrintSystem : public base::RefCountedThreadSafe<PrintSystem> {
 public:
  class PrintServerWatcher
      : public base::RefCountedThreadSafe<PrintServerWatcher> {
   public:
    // Callback interface for new printer notifications.
    class Delegate {
      public:
        virtual void OnPrinterAdded() = 0;
        // TODO(gene): Do we need OnPrinterDeleted notification here?

      protected:
        virtual ~Delegate() {}
    };

    virtual ~PrintServerWatcher() {}
    virtual bool StartWatching(PrintServerWatcher::Delegate* delegate) = 0;
    virtual bool StopWatching() = 0;
  };

  class PrinterWatcher : public base::RefCountedThreadSafe<PrinterWatcher> {
   public:
    // Callback interface for printer updates notifications.
    class Delegate {
      public:
        virtual void OnPrinterDeleted() = 0;
        virtual void OnPrinterChanged() = 0;
        virtual void OnJobChanged() = 0;

      protected:
        virtual ~Delegate() {}
    };

    virtual ~PrinterWatcher() {}
    virtual bool StartWatching(PrinterWatcher::Delegate* delegate) = 0;
    virtual bool StopWatching() = 0;
    virtual bool GetCurrentPrinterInfo(PrinterBasicInfo* printer_info) = 0;
  };

  class JobSpooler : public base::RefCountedThreadSafe<JobSpooler> {
   public:
    // Callback interface for JobSpooler notifications.
    class Delegate {
      public:
        virtual ~Delegate() { }
        virtual void OnJobSpoolSucceeded(const PlatformJobId& job_id) = 0;
        virtual void OnJobSpoolFailed() = 0;
    };

    virtual ~JobSpooler() {}
    // Spool job to the printer asynchronously. Caller will be notified via
    // |delegate|. Note that only one print job can be in progress at any given
    // time. Subsequent calls to Spool (before the Delegate::OnJobSpoolSucceeded
    // or Delegate::OnJobSpoolFailed methods are called) can fail.
    virtual bool Spool(const std::string& print_ticket,
                       const FilePath& print_data_file_path,
                       const std::string& print_data_mime_type,
                       const std::string& printer_name,
                       const std::string& job_title,
                       JobSpooler::Delegate* delegate) = 0;
  };

  virtual ~PrintSystem() {}

  // Enumerates the list of installed local and network printers.
  virtual void EnumeratePrinters(PrinterList* printer_list) = 0;

  // Gets the capabilities and defaults for a specific printer.
  virtual bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                     PrinterCapsAndDefaults* printer_info) = 0;

  // Returns true if ticket is valid.
  virtual bool ValidatePrintTicket(const std::string& printer_name,
                                   const std::string& print_ticket_data) = 0;

  // Get details for already spooled job.
  virtual bool GetJobDetails(const std::string& printer_name,
                             PlatformJobId job_id,
                             PrintJobDetails *job_details) = 0;

  // Returns true if printer_name points to a valid printer.
  virtual bool IsValidPrinter(const std::string& printer_name) = 0;

  // Factory methods to create corresponding watcher. Callee is responsible
  // for deleting objects. Return NULL if failed.
  virtual PrintServerWatcher* CreatePrintServerWatcher() = 0;
  virtual PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name) = 0;
  virtual JobSpooler* CreateJobSpooler() = 0;

  // Generate unique for proxy.
  static std::string GenerateProxyId();

  // Call this function to obtain printing system for specified print server.
  // If print settings are NULL, default settings will be used.
  // Return NULL if no print system available.
  static scoped_refptr<PrintSystem> CreateInstance(
      const DictionaryValue* print_system_settings);
};


// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef PrintSystem::PrintServerWatcher::Delegate PrintServerWatcherDelegate;
typedef PrintSystem::PrinterWatcher::Delegate PrinterWatcherDelegate;
typedef PrintSystem::JobSpooler::Delegate JobSpoolerDelegate;

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINT_SYSTEM_H_
