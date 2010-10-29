// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system.h"

#include <cups/cups.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

#include <list>
#include <map>

#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "printing/backend/cups_helper.h"
#include "printing/backend/print_backend.h"

namespace cloud_print {

static const char kCUPSPrinterInfoOpt[] = "printer-info";
static const char kCUPSPrinterStateOpt[] = "printer-state";
static const char kCUPSPrintServerURL[] = "print_server_url";

// Default port for IPP print servers.
static const int kDefaultIPPServerPort = 631;

class PrintSystemCUPS : public PrintSystem {
 public:
  PrintSystemCUPS(const GURL& print_server_url,
                  const DictionaryValue* print_system_settings);

  // PrintSystem implementation.
  virtual printing::PrintBackend* GetPrintBackend();

  virtual bool ValidatePrintTicket(const std::string& printer_name,
                                   const std::string& print_ticket_data);

  virtual bool GetJobDetails(const std::string& printer_name,
                             PlatformJobId job_id,
                             PrintJobDetails *job_details);

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
         : printer_name_(printer_name),
           delegate_(NULL),
           print_system_(print_system) {
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
    bool GetCurrentPrinterInfo(printing::PrinterBasicInfo* printer_info) {
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

  class JobSpoolerCUPS : public PrintSystem::JobSpooler {
   public:
    explicit JobSpoolerCUPS(PrintSystemCUPS* print_system)
        : print_system_(print_system) {
      DCHECK(print_system_.get());
    }
    // PrintSystem::JobSpooler implementation.
    virtual bool Spool(const std::string& print_ticket,
                       const FilePath& print_data_file_path,
                       const std::string& print_data_mime_type,
                       const std::string& printer_name,
                       const std::string& job_title,
                       JobSpooler::Delegate* delegate) {
      DCHECK(delegate);
      int job_id = print_system_->SpoolPrintJob(
          print_ticket, print_data_file_path, print_data_mime_type,
          printer_name, job_title);
      MessageLoop::current()->PostTask(FROM_HERE,
                                       NewRunnableFunction(
                                           &JobSpoolerCUPS::NotifyDelegate,
                                           delegate,
                                           job_id));
      return true;
    }

    static void NotifyDelegate(JobSpooler::Delegate* delegate, int job_id) {
      if (job_id)
        delegate->OnJobSpoolSucceeded(job_id);
      else
        delegate->OnJobSpoolFailed();
    }
   private:
    scoped_refptr<PrintSystemCUPS> print_system_;
    DISALLOW_COPY_AND_ASSIGN(JobSpoolerCUPS);
  };

  virtual PrintSystem::PrintServerWatcher* CreatePrintServerWatcher();
  virtual PrintSystem::PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name);
  virtual PrintSystem::JobSpooler* CreateJobSpooler();

  // Helper functions.
  PlatformJobId SpoolPrintJob(const std::string& print_ticket,
                              const FilePath& print_data_file_path,
                              const std::string& print_data_mime_type,
                              const std::string& printer_name,
                              const std::string& job_title);
  bool GetPrinterInfo(const std::string& printer_name,
                      printing::PrinterBasicInfo* info);
  bool ParsePrintTicket(const std::string& print_ticket,
                        std::map<std::string, std::string>* options);

 private:
  // Following functions are wrappers around corresponding CUPS functions.
  // <functions>2()  are called when print server is specified, and plain
  // version in another case. There is an issue specifing CUPS_HTTP_DEFAULT
  // in the <functions>2(), it does not work in CUPS prior to 1.4.
  int GetJobs(cups_job_t** jobs, const char* name,
              int myjobs, int whichjobs);
  int PrintFile(const char* name, const char* filename, const char* title,
                int num_options, cups_option_t* options);

  void Init(const DictionaryValue* print_system_settings);

  GURL print_server_url_;
  scoped_refptr<printing::PrintBackend> print_backend_;
};

PrintSystemCUPS::PrintSystemCUPS(const GURL& print_server_url,
                                 const DictionaryValue* print_system_settings)
    : print_server_url_(print_server_url) {
  Init(print_system_settings);
}

void PrintSystemCUPS::Init(const DictionaryValue* print_system_settings) {
  print_backend_ =
      printing::PrintBackend::CreateInstance(print_system_settings);
}

printing::PrintBackend* PrintSystemCUPS::GetPrintBackend() {
  return print_backend_;
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
    const std::string& key = *it;
    std::string value;
    if (ticket_dict->GetString(key, &value)) {
      (*options)[key] = value;
    }
  }

  return true;
}

bool PrintSystemCUPS::GetJobDetails(const std::string& printer_name,
                                    PlatformJobId job_id,
                                    PrintJobDetails *job_details) {
  DCHECK(job_details);

  cups_job_t* jobs = NULL;
  int num_jobs = GetJobs(&jobs, printer_name.c_str(), 1, -1);

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

  if (found)
    VLOG(1) << "CP_CUPS: Job details for: " << printer_name
            << " job_id: " << job_id << " job status: " << job_details->status;
  else
    LOG(WARNING) << "CP_CUPS: Job not found for: " << printer_name
                 << " job_id: " << job_id;

  cupsFreeJobs(num_jobs, jobs);
  return found;
}

bool PrintSystemCUPS::GetPrinterInfo(const std::string& printer_name,
                                     printing::PrinterBasicInfo* info) {
  DCHECK(info);

  VLOG(1) << "CP_CUPS: Getting printer info for: " << printer_name;

  // This is not very efficient way to get specific printer info. CUPS 1.4
  // supports cupsGetNamedDest() function. However, CUPS 1.4 is not available
  // everywhere (for example, it supported from Mac OS 10.6 only).
  printing::PrinterList printer_list;
  print_backend_->EnumeratePrinters(&printer_list);

  printing::PrinterList::iterator it;
  for (it = printer_list.begin(); it != printer_list.end(); ++it) {
    if (it->printer_name == printer_name) {
      *info = *it;
      return true;
    }
  }
  return false;
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

PrintSystem::JobSpooler* PrintSystemCUPS::CreateJobSpooler() {
  return new JobSpoolerCUPS(this);
}

std::string PrintSystem::GenerateProxyId() {
  // TODO(gene): This code should generate a unique id for proxy. ID should be
  // unique for this user. Rand may return the same number. We'll need to change
  // this in the future.
  std::string id("CP_PROXY_");
  id += base::Uint64ToString(base::RandUint64());
  return id;
}

scoped_refptr<PrintSystem> PrintSystem::CreateInstance(
    const DictionaryValue* print_system_settings) {
  std::string print_server_url_str;
  if (print_system_settings) {
    print_system_settings->GetString(kCUPSPrintServerURL,
                                     &print_server_url_str);
  }
  GURL print_server_url(print_server_url_str.c_str());
  return new PrintSystemCUPS(print_server_url, print_system_settings);
}

int PrintSystemCUPS::PrintFile(const char* name, const char* filename,
                               const char* title, int num_options,
                               cups_option_t* options) {
  if (print_server_url_.is_empty()) {  // Use default (local) print server.
    return cupsPrintFile(name, filename, title, num_options, options);
  } else {
    printing::HttpConnectionCUPS http(print_server_url_);
    return cupsPrintFile2(http.http(), name, filename,
                          title, num_options, options);
  }
}

int PrintSystemCUPS::GetJobs(cups_job_t** jobs, const char* name,
                             int myjobs, int whichjobs) {
  if (print_server_url_.is_empty()) {  // Use default (local) print server.
    return cupsGetJobs(jobs, name, myjobs, whichjobs);
  } else {
    printing::HttpConnectionCUPS http(print_server_url_);
    return cupsGetJobs2(http.http(), jobs, name, myjobs, whichjobs);
  }
}

PlatformJobId PrintSystemCUPS::SpoolPrintJob(
    const std::string& print_ticket,
    const FilePath& print_data_file_path,
    const std::string& print_data_mime_type,
    const std::string& printer_name,
    const std::string& job_title) {
  VLOG(1) << "CP_CUPS: Spooling print job for: " << printer_name;

  // We need to store options as char* string for the duration of the
  // cupsPrintFile2 call. We'll use map here to store options, since
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

  int job_id = PrintFile(printer_name.c_str(),
                         print_data_file_path.value().c_str(),
                         job_title.c_str(),
                         cups_options.size(),
                         &(cups_options[0]));

  VLOG(1) << "CP_CUPS: Job spooled, id: " << job_id;

  return job_id;
}

}  // namespace cloud_print
