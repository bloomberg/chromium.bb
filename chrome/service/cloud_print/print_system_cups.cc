// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system.h"

#include <cups/cups.h>
#include <list>
#include <map>

#include "base/json/json_reader.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"

namespace cloud_print {

static const char kCUPSPrinterInfoOpt[] = "printer-info";
static const char kCUPSPrinterStateOpt[] = "printer-state";

class PrintSystemCUPS : public PrintSystem {
 public:
  virtual void EnumeratePrinters(PrinterList* printer_list);

  virtual bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                         PrinterCapsAndDefaults* printer_info);

  virtual bool ValidatePrintTicket(const std::string& printer_name,
                                   const std::string& print_ticket_data);

  virtual bool SpoolPrintJob(const std::string& print_ticket,
                             const FilePath& print_data_file_path,
                             const std::string& print_data_mime_type,
                             const std::string& printer_name,
                             const std::string& job_title,
                             PlatformJobId* job_id_ret);

  virtual bool GetJobDetails(const std::string& printer_name,
                             PlatformJobId job_id,
                             PrintJobDetails *job_details);

  virtual bool IsValidPrinter(const std::string& printer_name);

  // TODO(gene): Add implementation for CUPS print server watcher.
  class PrintServerWatcherCUPS
    : public PrintSystem::PrintServerWatcher {
   public:
    PrintServerWatcherCUPS() {}

    // PrintSystem::PrintServerWatcher interface
    virtual bool StartWatching(
        PrintSystem::PrintServerWatcher::Delegate* delegate) {
      NOTIMPLEMENTED();
      return true;
    }
    virtual bool StopWatching() {
      NOTIMPLEMENTED();
      return true;
    }
  };

  class PrinterWatcherCUPS
      : public PrintSystem::PrinterWatcher {
   public:
     explicit PrinterWatcherCUPS(PrintSystemCUPS* print_system,
                                 const std::string& printer_name)
         : printer_name_(printer_name), print_system_(print_system) {
     }

    // PrintSystem::PrinterWatcher interface
    virtual bool StartWatching(
        PrintSystem::PrinterWatcher::Delegate* delegate) {
      if (delegate_ != NULL)
        StopWatching();
      delegate_ = delegate;
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          NewRunnableMethod(this,
              &PrintSystemCUPS::PrinterWatcherCUPS::Update), 5000);
      return true;
    }
    virtual bool StopWatching() {
      delegate_ = NULL;
      return true;
    }
    bool GetCurrentPrinterInfo(PrinterBasicInfo* printer_info) {
      DCHECK(printer_info);
      return print_system_->GetPrinterInfo(printer_name_, printer_info);
    }

    void Update() {
      if (delegate_ == NULL)
        return;  // Orphan call. We have been stopped already.
      // For CUPS proxy, we are going to fire OnJobChanged notification
      // periodically. Higher level will check if there are any outstanding
      // jobs for this printer and check their status. If printer has no
      // outstanding jobs, OnJobChanged() will do nothing.
      delegate_->OnJobChanged();
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          NewRunnableMethod(this,
              &PrintSystemCUPS::PrinterWatcherCUPS::Update),
          kNotificationTimeout);
    }
   private:
    static const int kNotificationTimeout = 5000;  // in ms
    std::string printer_name_;
    PrintSystem::PrinterWatcher::Delegate* delegate_;
    scoped_refptr<PrintSystemCUPS> print_system_;
    DISALLOW_COPY_AND_ASSIGN(PrinterWatcherCUPS);
  };

  virtual PrintSystem::PrintServerWatcher* CreatePrintServerWatcher();
  virtual PrintSystem::PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name);

  // Helper functions.
  bool GetPrinterInfo(const std::string& printer_name, PrinterBasicInfo* info);
  bool ParsePrintTicket(const std::string& print_ticket,
                        std::map<std::string, std::string>* options);
};

void PrintSystemCUPS::EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  printer_list->clear();

  cups_dest_t* destinations = NULL;
  int num_dests = cupsGetDests(&destinations);

  for (int printer_index = 0; printer_index < num_dests; printer_index++) {
    const cups_dest_t& printer = destinations[printer_index];

    PrinterBasicInfo printer_info;
    printer_info.printer_name = printer.name;

    const char* info = cupsGetOption(kCUPSPrinterInfoOpt,
        printer.num_options, printer.options);
    if (info != NULL)
      printer_info.printer_description = info;

    const char* state = cupsGetOption(kCUPSPrinterStateOpt,
        printer.num_options, printer.options);
    if (state != NULL)
      StringToInt(state, &printer_info.printer_status);

    // Store printer options.
    for (int opt_index = 0; opt_index < printer.num_options; opt_index++) {
      printer_info.options[printer.options[opt_index].name] =
          printer.options[opt_index].value;
    }

    printer_list->push_back(printer_info);
  }

  cupsFreeDests(num_dests, destinations);

  DLOG(INFO) << "CP_CUPS: Enumerated " << printer_list->size() << " printers.";
}

bool PrintSystemCUPS::GetPrinterCapsAndDefaults(const std::string& printer_name,
                                         PrinterCapsAndDefaults* printer_info) {
  DCHECK(printer_info);

  DLOG(INFO) << "CP_CUPS: Getting Caps and Defaults for: " << printer_name;

  static Lock ppd_lock;
  // cupsGetPPD returns a filename stored in a static buffer in CUPS.
  // Protect this code with lock.
  ppd_lock.Acquire();
  FilePath ppd_path;
  const char* ppd_file_path = cupsGetPPD(printer_name.c_str());
  if (ppd_file_path)
    ppd_path = FilePath(ppd_file_path);
  ppd_lock.Release();

  // In some cases CUPS failed to get ppd file.
  if (ppd_path.empty()) {
    DLOG(ERROR) << "CP_CUPS: Failed to get PPD for: " << printer_name;
    return false;
  }

  std::string content;
  bool res = file_util::ReadFileToString(ppd_path, &content);

  file_util::Delete(ppd_path, false);

  if (res) {
    printer_info->printer_capabilities.swap(content);
    printer_info->caps_mime_type = "application/pagemaker";
    // In CUPS, printer defaults is a part of PPD file. Nothing to upload here.
    printer_info->printer_defaults.clear();
    printer_info->defaults_mime_type.clear();
  }

  return res;
}

bool PrintSystemCUPS::ValidatePrintTicket(const std::string& printer_name,
                                        const std::string& print_ticket_data) {
  scoped_ptr<Value> ticket_value(base::JSONReader::Read(print_ticket_data,
      false));
  return ticket_value != NULL && ticket_value->IsType(Value::TYPE_DICTIONARY);
}

// Print ticket on linux is a JSON string containing only one dictionary.
bool PrintSystemCUPS::ParsePrintTicket(const std::string& print_ticket,
                                  std::map<std::string, std::string>* options) {
  DCHECK(options);
  scoped_ptr<Value> ticket_value(base::JSONReader::Read(print_ticket, false));
  if (ticket_value == NULL || !ticket_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  options->clear();
  DictionaryValue* ticket_dict =
      static_cast<DictionaryValue*>(ticket_value.get());
  DictionaryValue::key_iterator it(ticket_dict->begin_keys());
  for (; it != ticket_dict->end_keys(); ++it) {
    std::wstring key = *it;
    std::string value;
    if (ticket_dict->GetString(key, &value)) {
      (*options)[WideToUTF8(key.c_str())] = value;
    }
  }

  return true;
}

bool PrintSystemCUPS::SpoolPrintJob(const std::string& print_ticket,
                                    const FilePath& print_data_file_path,
                                    const std::string& print_data_mime_type,
                                    const std::string& printer_name,
                                    const std::string& job_title,
                                    PlatformJobId* job_id_ret) {
  DCHECK(job_id_ret);

  DLOG(INFO) << "CP_CUPS: Spooling print job for: " << printer_name;

  // We need to store options as char* string for the duration of the
  // cupsPrintFile call. We'll use map here to store options, since
  // Dictionary value from JSON parser returns wchat_t.
  std::map<std::string, std::string> options;
  bool res = ParsePrintTicket(print_ticket, &options);
  DCHECK(res);  // If print ticket is invalid we still print using defaults.

  std::vector<cups_option_t> cups_options;
  std::map<std::string, std::string>::iterator it;
  for (it = options.begin(); it != options.end(); ++it) {
    cups_option_t opt;
    opt.name = const_cast<char*>(it->first.c_str());
    opt.value = const_cast<char*>(it->second.c_str());
    cups_options.push_back(opt);
  }

  int job_id = cupsPrintFile(printer_name.c_str(),
                             print_data_file_path.value().c_str(),
                             job_title.c_str(),
                             cups_options.size(),
                             &(cups_options[0]));

  DLOG(INFO) << "CP_CUPS: Job spooled, id: " << job_id;

  if (job_id == 0)
    return false;

  *job_id_ret = job_id;
  return true;
}

bool PrintSystemCUPS::GetJobDetails(const std::string& printer_name,
                                    PlatformJobId job_id,
                                    PrintJobDetails *job_details) {
  DCHECK(job_details);

  DLOG(INFO) << "CP_CUPS: Getting job details for: " << printer_name <<
      " job_id: " << job_id;

  cups_job_t* jobs = NULL;
  int num_jobs = cupsGetJobs(&jobs, printer_name.c_str(), 1, -1);

  bool found = false;
  for (int i = 0; i < num_jobs; i++) {
    if (jobs[i].id == job_id) {
      found = true;
      switch (jobs[i].state) {
        case IPP_JOB_PENDING :
        case IPP_JOB_HELD :
        case IPP_JOB_PROCESSING :
          job_details->status = PRINT_JOB_STATUS_IN_PROGRESS;
          break;
        case IPP_JOB_STOPPED :
        case IPP_JOB_CANCELLED :
        case IPP_JOB_ABORTED :
          job_details->status = PRINT_JOB_STATUS_ERROR;
          break;
        case IPP_JOB_COMPLETED :
          job_details->status = PRINT_JOB_STATUS_COMPLETED;
          break;
        default:
          job_details->status = PRINT_JOB_STATUS_INVALID;
      }
      job_details->platform_status_flags = jobs[i].state;

      // We don't have any details on the number of processed pages here.
      break;
    }
  }

  cupsFreeJobs(num_jobs, jobs);
  return found;
}

bool PrintSystemCUPS::GetPrinterInfo(const std::string& printer_name,
                                     PrinterBasicInfo* info) {
  DCHECK(info);

  DLOG(INFO) << "CP_CUPS: Getting printer info for: " << printer_name;

  // This is not very efficient way to get specific printer info. CUPS 1.4
  // supports cupsGetNamedDest() function. However, CUPS 1.4 is not available
  // everywhere (for example, it supported from Mac OS 10.6 only).
  PrinterList printer_list;
  EnumeratePrinters(&printer_list);

  PrinterList::iterator it;
  for (it = printer_list.begin(); it != printer_list.end(); ++it) {
    if (it->printer_name == printer_name) {
      *info = *it;
      return true;
    }
  }
  return false;
}

bool PrintSystemCUPS::IsValidPrinter(const std::string& printer_name) {
  PrinterBasicInfo info;
  return GetPrinterInfo(printer_name, &info);
}

PrintSystem::PrintServerWatcher*
PrintSystemCUPS::CreatePrintServerWatcher() {
  return new PrintServerWatcherCUPS();
}

PrintSystem::PrinterWatcher* PrintSystemCUPS::CreatePrinterWatcher(
    const std::string& printer_name) {
  DCHECK(!printer_name.empty());
  return new PrinterWatcherCUPS(this, printer_name);
}

std::string PrintSystem::GenerateProxyId() {
  // TODO(gene): This code should generate a unique id for proxy. ID should be
  // unique for this user. Rand may return the same number. We'll need to change
  // this in the future.
  std::string id("CP_PROXY_");
  id += Uint64ToString(base::RandUint64());
  return id;
}

scoped_refptr<PrintSystem> PrintSystem::CreateInstance() {
  return new PrintSystemCUPS;
}

}  // namespace cloud_print

