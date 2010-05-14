// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/printer_info.h"

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

void EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  printer_list->clear();

  cups_dest_t* destinations = NULL;
  int num_dests = cupsGetDests(&destinations);

  for (int i = 0; i < num_dests; i++) {
    PrinterBasicInfo printer_info;
    printer_info.printer_name = destinations[i].name;

    const char* info = cupsGetOption(kCUPSPrinterInfoOpt,
        destinations[i].num_options, destinations[i].options);
    if (info != NULL)
      printer_info.printer_description = info;

    const char* state = cupsGetOption(kCUPSPrinterStateOpt,
        destinations[i].num_options, destinations[i].options);
    if (state != NULL)
      StringToInt(state, &printer_info.printer_status);

    printer_list->push_back(printer_info);
  }

  cupsFreeDests(num_dests, destinations);
}

bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                               PrinterCapsAndDefaults* printer_info) {
  DCHECK(printer_info);

  static Lock ppd_lock;
  // cupsGetPPD returns a filename stored in a static buffer in CUPS.
  // Protect this code with lock.
  ppd_lock.Acquire();
  FilePath ppd_path(cupsGetPPD(printer_name.c_str()));
  ppd_lock.Release();

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

bool ValidatePrintTicket(const std::string& printer_name,
                         const std::string& print_ticket_data) {
  scoped_ptr<Value> ticket_value(base::JSONReader::Read(print_ticket_data,
      false));
  return ticket_value != NULL && ticket_value->IsType(Value::TYPE_DICTIONARY);
}

std::string GenerateProxyId() {
  // TODO(gene): This code should generate a unique id for proxy. ID should be
  // unique for this user. Rand may return the same number. We'll need to change
  // this in the future.
  std::string id("CP_PROXY_");
  id += Uint64ToString(base::RandUint64());
  return id;
}

// Print ticket on linux is a JSON string containing only one dictionary.
bool ParsePrintTicket(const std::string& print_ticket,
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

bool SpoolPrintJob(const std::string& print_ticket,
                   const FilePath& print_data_file_path,
                   const std::string& print_data_mime_type,
                   const std::string& printer_name,
                   const std::string& job_title,
                   PlatformJobId* job_id_ret) {
  DCHECK(job_id_ret);

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

  if (job_id == 0)
    return false;

  *job_id_ret = job_id;
  return true;
}

bool GetJobDetails(const std::string& printer_name,
                   PlatformJobId job_id,
                   PrintJobDetails *job_details) {
  DCHECK(job_details);
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

bool GetPrinterInfo(const std::string& printer_name, PrinterBasicInfo* info) {
  DCHECK(info);

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

bool IsValidPrinter(const std::string& printer_name) {
  PrinterBasicInfo info;
  return GetPrinterInfo(printer_name, &info);
}

class PrinterChangeNotifier::NotificationState
    : public base::RefCountedThreadSafe<
        PrinterChangeNotifier::NotificationState> {
 public:
  NotificationState() : delegate_(NULL) {}
  bool Start(const std::string& printer_name,
             PrinterChangeNotifier::Delegate* delegate) {
    if (delegate_ != NULL)
      Stop();

    printer_name_ = printer_name;
    delegate_ = delegate;

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this,
            &PrinterChangeNotifier::NotificationState::Update), 5000);
    return true;
  }
  bool Stop() {
    delegate_ = NULL;
    return true;
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
            &PrinterChangeNotifier::NotificationState::Update),
        kNotificationTimeout);
  }
  std::string printer_name() const {
    return printer_name_;
  }
 private:
  friend class base::RefCountedThreadSafe<
      PrinterChangeNotifier::NotificationState>;
  ~NotificationState() {
    Stop();
  }
  static const int kNotificationTimeout = 5000;  // in ms
  std::string printer_name_;  // The printer being watched
  PrinterChangeNotifier::Delegate* delegate_;  // Delegate to notify
};

PrinterChangeNotifier::PrinterChangeNotifier() : state_(NULL) {
}

PrinterChangeNotifier::~PrinterChangeNotifier() {
  StopWatching();
}

bool PrinterChangeNotifier::StartWatching(const std::string& printer_name,
                                          Delegate* delegate) {
  if (state_) {
    NOTREACHED();
    return false;
  }
  state_ = new NotificationState;
  state_->AddRef();
  if (!state_->Start(printer_name, delegate)) {
    StopWatching();
    return false;
  }
  return true;
}

bool PrinterChangeNotifier::StopWatching() {
  if (!state_) {
    return false;
  }
  state_->Stop();
  state_->Release();
  state_ = NULL;
  return true;
}

bool PrinterChangeNotifier::GetCurrentPrinterInfo(
    PrinterBasicInfo* printer_info) {
  if (!state_) {
    return false;
  }
  DCHECK(printer_info);
  return GetPrinterInfo(state_->printer_name(), printer_info);
}
}  // namespace cloud_print

