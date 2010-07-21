// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system.h"

#include <cups/cups.h>
#include <dlfcn.h>
#include <errno.h>
#include <gcrypt.h>
#include <pthread.h>

#include <list>
#include <map>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"

namespace {

// Init GCrypt library (needed for CUPS) using pthreads.
// I've hit a bug in CUPS library, when it crashed with: "ath.c:184:
// _gcry_ath_mutex_lock: Assertion `*lock == ((ath_mutex_t) 0)' failed."
// It happened whe multiple threads tried printing simultaneously.
// Google search for 'gnutls thread safety' provided with following solution
// where we initialize gcrypt by initializing gnutls.
//
// Initially, we linked with -lgnutls and simply called gnutls_global_init(),
// but this did not work well since we build one binary on Ubuntu Hardy and
// expect it to run on many Linux distros. (See http://crbug.com/46954)
// So instead we use dlopen() and dlsym() to dynamically load and call
// gnutls_global_init().

GCRY_THREAD_OPTION_PTHREAD_IMPL;

bool init_gnutls() {
  const char* kGnuTlsFile = "libgnutls.so";
  void* gnutls_lib = dlopen(kGnuTlsFile, RTLD_NOW);
  if (!gnutls_lib) {
    LOG(ERROR) << "Cannot load " << kGnuTlsFile;
    return false;
  }
  const char* kGnuTlsInitFuncName = "gnutls_global_init";
  int (*pgnutls_global_init)(void) = reinterpret_cast<int(*)()>(
      dlsym(gnutls_lib, kGnuTlsInitFuncName));
  if (!pgnutls_global_init) {
    LOG(ERROR) << "Could not find " << kGnuTlsInitFuncName
               << " in " << kGnuTlsFile;
    return false;
  }
  return ((*pgnutls_global_init)() == 0);
}

void init_gcrypt() {
  // The gnutls_global_init() man page warns it's not thread safe. Locking this
  // entire function just to be on the safe side.
  static Lock init_gcrypt_lock;
  AutoLock init_gcrypt_autolock(init_gcrypt_lock);
  static bool gcrypt_initialized = false;
  if (!gcrypt_initialized) {
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
    gcrypt_initialized = init_gnutls();
    if (!gcrypt_initialized) {
      LOG(ERROR) << "Gcrypt initialization failed";
    }
  }
}

}  // namespace

namespace cloud_print {

static const char kCUPSPrinterInfoOpt[] = "printer-info";
static const char kCUPSPrinterStateOpt[] = "printer-state";
static const wchar_t kCUPSPrintServerURL[] = L"print_server_url";

// Default port for IPP print servers.
static const int kDefaultIPPServerPort = 631;

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
class HttpConnectionCUPS {
 public:
  explicit HttpConnectionCUPS(const GURL& print_server_url) : http_(NULL) {
    // If we have an empty url, use default print server.
    if (print_server_url.is_empty())
      return;

    int port = print_server_url.IntPort();
    if (port == url_parse::PORT_UNSPECIFIED)
      port = kDefaultIPPServerPort;

    http_ = httpConnectEncrypt(print_server_url.host().c_str(), port,
                               HTTP_ENCRYPT_NEVER);
    if (http_ == NULL) {
      LOG(ERROR) << "CP_CUPS: Failed connecting to print server: " <<
                 print_server_url;
    }
  }

  ~HttpConnectionCUPS() {
    if (http_ != NULL)
      httpClose(http_);
  }

  http_t* http() {
    return http_;
  }

 private:
  http_t* http_;
};

class PrintSystemCUPS : public PrintSystem {
 public:
  explicit PrintSystemCUPS(const GURL& print_server_url);

  // PrintSystem implementation.
  virtual void EnumeratePrinters(PrinterList* printer_list);

  virtual bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                         PrinterCapsAndDefaults* printer_info);

  virtual bool ValidatePrintTicket(const std::string& printer_name,
                                   const std::string& print_ticket_data);

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
  bool GetPrinterInfo(const std::string& printer_name, PrinterBasicInfo* info);
  bool ParsePrintTicket(const std::string& print_ticket,
                        std::map<std::string, std::string>* options);

 private:
  // Following functions are wrappers around corresponding CUPS functions.
  // <functions>2()  are called when print server is specified, and plain
  // version in another case. There is an issue specifing CUPS_HTTP_DEFAULT
  // in the <functions>2(), it does not work in CUPS prior to 1.4.
  int GetDests(cups_dest_t** dests);
  FilePath GetPPD(const char* name);
  int GetJobs(cups_job_t** jobs, const char* name,
              int myjobs, int whichjobs);
  int PrintFile(const char* name, const char* filename, const char* title,
                int num_options, cups_option_t* options);

  GURL print_server_url_;
};

PrintSystemCUPS::PrintSystemCUPS(const GURL& print_server_url)
    : print_server_url_(print_server_url) {
}

void PrintSystemCUPS::EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  printer_list->clear();

  cups_dest_t* destinations = NULL;
  int num_dests = GetDests(&destinations);

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

  LOG(INFO) << "CP_CUPS: Enumerated " << printer_list->size() << " printers.";
}

bool PrintSystemCUPS::GetPrinterCapsAndDefaults(const std::string& printer_name,
                                         PrinterCapsAndDefaults* printer_info) {
  DCHECK(printer_info);

  LOG(INFO) << "CP_CUPS: Getting Caps and Defaults for: " << printer_name;

  FilePath ppd_path(GetPPD(printer_name.c_str()));
  // In some cases CUPS failed to get ppd file.
  if (ppd_path.empty()) {
    LOG(ERROR) << "CP_CUPS: Failed to get PPD for: " << printer_name;
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
    LOG(INFO) << "CP_CUPS: Job details for: " << printer_name <<
        " job_id: " << job_id << " job status: " << job_details->status;
  else
    LOG(WARNING) << "CP_CUPS: Job not found for: " << printer_name <<
        " job_id: " << job_id;

  cupsFreeJobs(num_jobs, jobs);
  return found;
}

bool PrintSystemCUPS::GetPrinterInfo(const std::string& printer_name,
                                     PrinterBasicInfo* info) {
  DCHECK(info);

  LOG(INFO) << "CP_CUPS: Getting printer info for: " << printer_name;

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

PrintSystem::JobSpooler* PrintSystemCUPS::CreateJobSpooler() {
  return new JobSpoolerCUPS(this);
}

std::string PrintSystem::GenerateProxyId() {
  // TODO(gene): This code should generate a unique id for proxy. ID should be
  // unique for this user. Rand may return the same number. We'll need to change
  // this in the future.
  std::string id("CP_PROXY_");
  id += Uint64ToString(base::RandUint64());
  return id;
}

scoped_refptr<PrintSystem> PrintSystem::CreateInstance(
    const DictionaryValue* print_system_settings) {
  // Initialize gcrypt library.
  init_gcrypt();

  std::string print_server_url_str;
  if (print_system_settings) {
    print_system_settings->GetString(
        kCUPSPrintServerURL, &print_server_url_str);
  }
  GURL print_server_url(print_server_url_str.c_str());
  return new PrintSystemCUPS(print_server_url);
}

int PrintSystemCUPS::GetDests(cups_dest_t** dests) {
  if (print_server_url_.is_empty()) {  // Use default (local) print server.
    return cupsGetDests(dests);
  } else {
    HttpConnectionCUPS http(print_server_url_);
    return cupsGetDests2(http.http(), dests);
  }
}

FilePath PrintSystemCUPS::GetPPD(const char* name) {
  // cupsGetPPD returns a filename stored in a static buffer in CUPS.
  // Protect this code with lock.
  static Lock ppd_lock;
  AutoLock ppd_autolock(ppd_lock);
  FilePath ppd_path;
  const char* ppd_file_path = NULL;
  if (print_server_url_.is_empty()) {  // Use default (local) print server.
    ppd_file_path = cupsGetPPD(name);
  } else {
    HttpConnectionCUPS http(print_server_url_);
    ppd_file_path = cupsGetPPD2(http.http(), name);
  }
  if (ppd_file_path)
    ppd_path = FilePath(ppd_file_path);
  return ppd_path;
}

int PrintSystemCUPS::PrintFile(const char* name, const char* filename,
                               const char* title, int num_options,
                               cups_option_t* options) {
  if (print_server_url_.is_empty()) {  // Use default (local) print server.
    return cupsPrintFile(name, filename, title, num_options, options);
  } else {
    HttpConnectionCUPS http(print_server_url_);
    return cupsPrintFile2(http.http(), name, filename,
                          title, num_options, options);
  }
}

int PrintSystemCUPS::GetJobs(cups_job_t** jobs, const char* name,
                             int myjobs, int whichjobs) {
  if (print_server_url_.is_empty()) {  // Use default (local) print server.
    return cupsGetJobs(jobs, name, myjobs, whichjobs);
  } else {
    HttpConnectionCUPS http(print_server_url_);
    return cupsGetJobs2(http.http(), jobs, name, myjobs, whichjobs);
  }
}

PlatformJobId PrintSystemCUPS::SpoolPrintJob(
    const std::string& print_ticket,
    const FilePath& print_data_file_path,
    const std::string& print_data_mime_type,
    const std::string& printer_name,
    const std::string& job_title) {
  LOG(INFO) << "CP_CUPS: Spooling print job for: " << printer_name;

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

  LOG(INFO) << "CP_CUPS: Job spooled, id: " << job_id;

  return job_id;
}

}  // namespace cloud_print
