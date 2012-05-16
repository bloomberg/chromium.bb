// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system.h"

#include <cups/cups.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

#include <algorithm>
#include <list>
#include <map>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "printing/backend/cups_helper.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// CUPS specific options.
const char kCUPSPrinterInfoOpt[] = "printer-info";
const char kCUPSPrinterStateOpt[] = "printer-state";

// Print system config options.
const char kCUPSPrintServerURLs[] = "print_server_urls";
const char kCUPSUpdateTimeoutMs[] = "update_timeout_ms";
const char kCUPSNotifyDelete[] = "notify_delete";

// Default port for IPP print servers.
const int kDefaultIPPServerPort = 631;

// Time interval to check for printer's updates.
const int kCheckForPrinterUpdatesMinutes = 5;

// Job update timeout
const int kJobUpdateTimeoutSeconds = 5;

// Job id for dry run (it should not affect CUPS job ids, since 0 job-id is
// invalid in CUPS.
const int kDryRunJobId = 0;

}  // namespace

namespace cloud_print {

struct PrintServerInfoCUPS {
  GURL url;
  scoped_refptr<printing::PrintBackend> backend;
  printing::PrinterList printers;
  // CapsMap cache PPD until the next update and give a fast access to it by
  // printer name. PPD request is relatively expensive and this should minimize
  // the number of requests.
  typedef std::map<std::string, printing::PrinterCapsAndDefaults> CapsMap;
  CapsMap caps_cache;
};

class PrintSystemCUPS : public PrintSystem {
 public:
  explicit PrintSystemCUPS(const DictionaryValue* print_system_settings);

  // PrintSystem implementation.
  virtual PrintSystemResult Init() OVERRIDE;
  virtual PrintSystem::PrintSystemResult EnumeratePrinters(
      printing::PrinterList* printer_list) OVERRIDE;
  virtual void GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      const PrinterCapsAndDefaultsCallback& callback) OVERRIDE;
  virtual bool IsValidPrinter(const std::string& printer_name) OVERRIDE;
  virtual bool ValidatePrintTicket(
      const std::string& printer_name,
      const std::string& print_ticket_data) OVERRIDE;
  virtual bool GetJobDetails(const std::string& printer_name,
                             PlatformJobId job_id,
                             PrintJobDetails *job_details) OVERRIDE;
  virtual PrintSystem::PrintServerWatcher* CreatePrintServerWatcher() OVERRIDE;
  virtual PrintSystem::PrinterWatcher* CreatePrinterWatcher(
      const std::string& printer_name) OVERRIDE;
  virtual PrintSystem::JobSpooler* CreateJobSpooler() OVERRIDE;
  virtual std::string GetSupportedMimeTypes() OVERRIDE;

  // Helper functions.
  PlatformJobId SpoolPrintJob(const std::string& print_ticket,
                              const FilePath& print_data_file_path,
                              const std::string& print_data_mime_type,
                              const std::string& printer_name,
                              const std::string& job_title,
                              const std::vector<std::string>& tags,
                              bool* dry_run);
  bool GetPrinterInfo(const std::string& printer_name,
                      printing::PrinterBasicInfo* info);
  bool ParsePrintTicket(const std::string& print_ticket,
                        std::map<std::string, std::string>* options);

  // Synchronous version of GetPrinterCapsAndDefaults.
  bool GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      printing::PrinterCapsAndDefaults* printer_info);

  base::TimeDelta GetUpdateTimeout() const {
    return update_timeout_;
  }

  bool NotifyDelete() const {
    // Notify about deleted printers only when we
    // fetched printers list without errors.
    return notify_delete_ && printer_enum_succeeded_;
  }

 private:
  virtual ~PrintSystemCUPS() {}

  // Following functions are wrappers around corresponding CUPS functions.
  // <functions>2()  are called when print server is specified, and plain
  // version in another case. There is an issue specifing CUPS_HTTP_DEFAULT
  // in the <functions>2(), it does not work in CUPS prior to 1.4.
  int GetJobs(cups_job_t** jobs, const GURL& url,
              http_encryption_t encryption, const char* name,
              int myjobs, int whichjobs);
  int PrintFile(const GURL& url, http_encryption_t encryption,
                const char* name, const char* filename,
                const char* title, int num_options, cups_option_t* options);

  void InitPrintBackends(const DictionaryValue* print_system_settings);
  void AddPrintServer(const std::string& url);

  void UpdatePrinters();

  // Full name contains print server url:port and printer name. Short name
  // is the name of the printer in the CUPS server.
  std::string MakeFullPrinterName(const GURL& url,
                                  const std::string& short_printer_name);
  PrintServerInfoCUPS* FindServerByFullName(
      const std::string& full_printer_name, std::string* short_printer_name);

  // Helper method to invoke a PrinterCapsAndDefaultsCallback.
  static void RunCapsCallback(
      const PrinterCapsAndDefaultsCallback& callback,
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& printer_info);

  // PrintServerList contains information about all print servers and backends
  // this proxy is connected to.
  typedef std::list<PrintServerInfoCUPS> PrintServerList;
  PrintServerList print_servers_;

  base::TimeDelta update_timeout_;
  bool initialized_;
  bool printer_enum_succeeded_;
  bool notify_delete_;
  http_encryption_t cups_encryption_;
};

class PrintServerWatcherCUPS
  : public PrintSystem::PrintServerWatcher {
 public:
  explicit PrintServerWatcherCUPS(PrintSystemCUPS* print_system)
      : print_system_(print_system),
        delegate_(NULL) {
  }

  // PrintSystem::PrintServerWatcher implementation.
  virtual bool StartWatching(
      PrintSystem::PrintServerWatcher::Delegate* delegate) OVERRIDE {
    delegate_ = delegate;
    printers_hash_ = GetPrintersHash();
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrintServerWatcherCUPS::CheckForUpdates, this),
        print_system_->GetUpdateTimeout());
    return true;
  }

  virtual bool StopWatching() OVERRIDE {
    delegate_ = NULL;
    return true;
  }

  void CheckForUpdates() {
    if (delegate_ == NULL)
      return;  // Orphan call. We have been stopped already.
    VLOG(1) << "CP_CUPS: Checking for new printers";
    std::string new_hash = GetPrintersHash();
    if (printers_hash_ != new_hash) {
      printers_hash_ = new_hash;
      delegate_->OnPrinterAdded();
    }
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrintServerWatcherCUPS::CheckForUpdates, this),
        print_system_->GetUpdateTimeout());
  }

 protected:
  virtual ~PrintServerWatcherCUPS() {
    StopWatching();
  }

 private:
  std::string GetPrintersHash() {
    printing::PrinterList printer_list;
    print_system_->EnumeratePrinters(&printer_list);

    // Sort printer names.
    std::vector<std::string> printers;
    printing::PrinterList::iterator it;
    for (it = printer_list.begin(); it != printer_list.end(); ++it)
      printers.push_back(it->printer_name);
    std::sort(printers.begin(), printers.end());

    std::string to_hash;
    for (size_t i = 0; i < printers.size(); i++)
      to_hash += printers[i];

    return base::MD5String(to_hash);
  }

  scoped_refptr<PrintSystemCUPS> print_system_;
  PrintSystem::PrintServerWatcher::Delegate* delegate_;
  std::string printers_hash_;

  DISALLOW_COPY_AND_ASSIGN(PrintServerWatcherCUPS);
};

class PrinterWatcherCUPS
    : public PrintSystem::PrinterWatcher {
 public:
  PrinterWatcherCUPS(PrintSystemCUPS* print_system,
                     const std::string& printer_name)
      : printer_name_(printer_name),
        delegate_(NULL),
        print_system_(print_system) {
  }

  // PrintSystem::PrinterWatcher implementation.
  virtual bool StartWatching(
      PrintSystem::PrinterWatcher::Delegate* delegate) OVERRIDE{
    scoped_refptr<printing::PrintBackend> print_backend(
        printing::PrintBackend::CreateInstance(NULL));
    child_process_logging::ScopedPrinterInfoSetter prn_info(
        print_backend->GetPrinterDriverInfo(printer_name_));
    if (delegate_ != NULL)
      StopWatching();
    delegate_ = delegate;
    settings_hash_ = GetSettingsHash();
    // Schedule next job status update.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrinterWatcherCUPS::JobStatusUpdate, this),
        base::TimeDelta::FromSeconds(kJobUpdateTimeoutSeconds));
    // Schedule next printer check.
    // TODO(gene): Randomize time for the next printer update.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrinterWatcherCUPS::PrinterUpdate, this),
        print_system_->GetUpdateTimeout());
    return true;
  }

  virtual bool StopWatching() OVERRIDE{
    delegate_ = NULL;
    return true;
  }

  bool GetCurrentPrinterInfo(printing::PrinterBasicInfo* printer_info) {
    DCHECK(printer_info);
    return print_system_->GetPrinterInfo(printer_name_, printer_info);
  }

  void JobStatusUpdate() {
    if (delegate_ == NULL)
      return;  // Orphan call. We have been stopped already.
    // For CUPS proxy, we are going to fire OnJobChanged notification
    // periodically. Higher level will check if there are any outstanding
    // jobs for this printer and check their status. If printer has no
    // outstanding jobs, OnJobChanged() will do nothing.
    delegate_->OnJobChanged();
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrinterWatcherCUPS::JobStatusUpdate, this),
        base::TimeDelta::FromSeconds(kJobUpdateTimeoutSeconds));
  }

  void PrinterUpdate() {
    if (delegate_ == NULL)
      return;  // Orphan call. We have been stopped already.
    VLOG(1) << "CP_CUPS: Checking for printer updates: " << printer_name_;
    if (print_system_->NotifyDelete() &&
        !print_system_->IsValidPrinter(printer_name_)) {
      delegate_->OnPrinterDeleted();
      VLOG(1) << "CP_CUPS: Printer deleted: " << printer_name_;
    } else {
      std::string new_hash = GetSettingsHash();
      if (settings_hash_ != new_hash) {
        settings_hash_ = new_hash;
        delegate_->OnPrinterChanged();
        VLOG(1) << "CP_CUPS: Printer update detected for: " << printer_name_;
      }
    }
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrinterWatcherCUPS::PrinterUpdate, this),
        print_system_->GetUpdateTimeout());
  }

 protected:
  virtual ~PrinterWatcherCUPS() {
    StopWatching();
  }

 private:
  std::string GetSettingsHash() {
    printing::PrinterBasicInfo info;
    if (!print_system_->GetPrinterInfo(printer_name_, &info))
      return std::string();

    printing::PrinterCapsAndDefaults caps;
    if (!print_system_->GetPrinterCapsAndDefaults(printer_name_, &caps))
      return std::string();

    std::string to_hash(info.printer_name);
    to_hash += info.printer_description;
    std::map<std::string, std::string>::const_iterator it;
    for (it = info.options.begin(); it != info.options.end(); ++it) {
      to_hash += it->first;
      to_hash += it->second;
    }

    to_hash += caps.printer_capabilities;
    to_hash += caps.caps_mime_type;
    to_hash += caps.printer_defaults;
    to_hash += caps.defaults_mime_type;

    return base::MD5String(to_hash);
  }
  std::string printer_name_;
  PrintSystem::PrinterWatcher::Delegate* delegate_;
  scoped_refptr<PrintSystemCUPS> print_system_;
  std::string settings_hash_;

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
                     const std::vector<std::string>& tags,
                     JobSpooler::Delegate* delegate) OVERRIDE{
    DCHECK(delegate);
    bool dry_run = false;
    int job_id = print_system_->SpoolPrintJob(
        print_ticket, print_data_file_path, print_data_mime_type,
        printer_name, job_title, tags, &dry_run);
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&JobSpoolerCUPS::NotifyDelegate, delegate, job_id, dry_run));
    return true;
  }

  static void NotifyDelegate(JobSpooler::Delegate* delegate,
                             int job_id, bool dry_run) {
    if (dry_run || job_id)
      delegate->OnJobSpoolSucceeded(job_id);
    else
      delegate->OnJobSpoolFailed();
  }

 protected:
  virtual ~JobSpoolerCUPS() {}

 private:
  scoped_refptr<PrintSystemCUPS> print_system_;

  DISALLOW_COPY_AND_ASSIGN(JobSpoolerCUPS);
};

PrintSystemCUPS::PrintSystemCUPS(const DictionaryValue* print_system_settings)
    : update_timeout_(base::TimeDelta::FromMinutes(
        kCheckForPrinterUpdatesMinutes)),
      initialized_(false),
      printer_enum_succeeded_(false),
      notify_delete_(true),
      cups_encryption_(HTTP_ENCRYPT_NEVER) {
  if (print_system_settings) {
    int timeout;
    if (print_system_settings->GetInteger(kCUPSUpdateTimeoutMs, &timeout))
      update_timeout_ = base::TimeDelta::FromMilliseconds(timeout);

    int encryption;
    if (print_system_settings->GetInteger(kCUPSEncryption, &encryption))
      cups_encryption_ =
          static_cast<http_encryption_t>(encryption);

    bool notify_delete = true;
    if (print_system_settings->GetBoolean(kCUPSNotifyDelete, &notify_delete))
      notify_delete_ = notify_delete;
  }

  InitPrintBackends(print_system_settings);
}

void PrintSystemCUPS::InitPrintBackends(
    const DictionaryValue* print_system_settings) {
  ListValue* url_list;
  if (print_system_settings &&
      print_system_settings->GetList(kCUPSPrintServerURLs, &url_list)) {
    for (size_t i = 0; i < url_list->GetSize(); i++) {
      std::string print_server_url;
      if (url_list->GetString(i, &print_server_url))
        AddPrintServer(print_server_url);
    }
  }

  // If server list is empty, use default print server.
  if (print_servers_.empty())
    AddPrintServer(std::string());
}

void PrintSystemCUPS::AddPrintServer(const std::string& url) {
  if (url.empty())
    LOG(WARNING) << "No print server specified. Using default print server.";

  // Get Print backend for the specific print server.
  DictionaryValue backend_settings;
  backend_settings.SetString(kCUPSPrintServerURL, url);

  // Make CUPS requests non-blocking.
  backend_settings.SetString(kCUPSBlocking, kValueFalse);

  // Set encryption for backend.
  backend_settings.SetInteger(kCUPSEncryption, cups_encryption_);

  PrintServerInfoCUPS print_server;
  print_server.backend =
    printing::PrintBackend::CreateInstance(&backend_settings);
  print_server.url = GURL(url.c_str());

  print_servers_.push_back(print_server);
}

PrintSystem::PrintSystemResult PrintSystemCUPS::Init() {
  UpdatePrinters();
  initialized_ = true;
  return PrintSystemResult(true, std::string());
}

void PrintSystemCUPS::UpdatePrinters() {
  PrintServerList::iterator it;
  printer_enum_succeeded_ = true;
  for (it = print_servers_.begin(); it != print_servers_.end(); ++it) {
    if (!it->backend->EnumeratePrinters(&it->printers))
      printer_enum_succeeded_ = false;
    it->caps_cache.clear();
    printing::PrinterList::iterator printer_it;
    for (printer_it = it->printers.begin();
        printer_it != it->printers.end(); ++printer_it) {
      printer_it->printer_name = MakeFullPrinterName(it->url,
                                                     printer_it->printer_name);
    }
    VLOG(1) << "CUPS: Updated printer list for url: " << it->url
            << " Number of printers: " << it->printers.size();
  }

  // Schedule next update.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PrintSystemCUPS::UpdatePrinters, this), GetUpdateTimeout());
}

PrintSystem::PrintSystemResult PrintSystemCUPS::EnumeratePrinters(
    printing::PrinterList* printer_list) {
  DCHECK(initialized_);
  printer_list->clear();
  PrintServerList::iterator it;
  for (it = print_servers_.begin(); it != print_servers_.end(); ++it) {
    printer_list->insert(printer_list->end(),
        it->printers.begin(), it->printers.end());
  }
  VLOG(1) << "CUPS: Total " << printer_list->size() << " printers enumerated.";
  // TODO(sanjeevr): Maybe some day we want to report the actual server names
  // for which the enumeration failed.
  return PrintSystemResult(printer_enum_succeeded_, std::string());
}

void PrintSystemCUPS::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    const PrinterCapsAndDefaultsCallback& callback) {
  printing::PrinterCapsAndDefaults printer_info;
  bool succeeded = GetPrinterCapsAndDefaults(printer_name, &printer_info);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PrintSystemCUPS::RunCapsCallback, callback, succeeded,
                 printer_name, printer_info));
}

bool PrintSystemCUPS::IsValidPrinter(const std::string& printer_name) {
  return GetPrinterInfo(printer_name, NULL);
}

bool PrintSystemCUPS::ValidatePrintTicket(const std::string& printer_name,
                                        const std::string& print_ticket_data) {
  DCHECK(initialized_);
  scoped_ptr<Value> ticket_value(base::JSONReader::Read(print_ticket_data));
  return ticket_value != NULL && ticket_value->IsType(Value::TYPE_DICTIONARY);
}

// Print ticket on linux is a JSON string containing only one dictionary.
bool PrintSystemCUPS::ParsePrintTicket(
    const std::string& print_ticket,
    std::map<std::string, std::string>* options) {
  DCHECK(options);
  scoped_ptr<Value> ticket_value(base::JSONReader::Read(print_ticket));
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

bool PrintSystemCUPS::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    printing::PrinterCapsAndDefaults* printer_info) {
  DCHECK(initialized_);
  std::string short_printer_name;
  PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  PrintServerInfoCUPS::CapsMap::iterator caps_it =
      server_info->caps_cache.find(printer_name);
  if (caps_it != server_info->caps_cache.end()) {
    *printer_info = caps_it->second;
    return true;
  }

  // TODO(gene): Retry multiple times in case of error.
  child_process_logging::ScopedPrinterInfoSetter prn_info(
      server_info->backend->GetPrinterDriverInfo(short_printer_name));
  if (!server_info->backend->GetPrinterCapsAndDefaults(short_printer_name,
                                                       printer_info) ) {
    return false;
  }

  server_info->caps_cache[printer_name] = *printer_info;
  return true;
}

bool PrintSystemCUPS::GetJobDetails(const std::string& printer_name,
                                    PlatformJobId job_id,
                                    PrintJobDetails *job_details) {
  DCHECK(initialized_);
  DCHECK(job_details);

  std::string short_printer_name;
  PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  child_process_logging::ScopedPrinterInfoSetter prn_info(
      server_info->backend->GetPrinterDriverInfo(short_printer_name));
  cups_job_t* jobs = NULL;
  int num_jobs = GetJobs(&jobs, server_info->url, cups_encryption_,
                         short_printer_name.c_str(), 1, -1);
  bool error = (num_jobs == 0) && (cupsLastError() > IPP_OK_EVENTS_COMPLETE);
  if (error) {
    VLOG(1) << "CP_CUPS: Error getting jobs from CUPS server. Printer:"
            << printer_name
            << " Error: "
            << static_cast<int>(cupsLastError());
    return false;
  }

  // Check if the request is for dummy dry run job.
  // We check this after calling GetJobs API to see if this printer is actually
  // accessible through CUPS.
  if (job_id == kDryRunJobId) {
    job_details->status = PRINT_JOB_STATUS_COMPLETED;
    VLOG(1) << "CP_CUPS: Dry run job succeeded for: " << printer_name;
    return true;
  }

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
  DCHECK(initialized_);
  if (info)
    VLOG(1) << "CP_CUPS: Getting printer info for: " << printer_name;

  std::string short_printer_name;
  PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  printing::PrinterList::iterator it;
  for (it = server_info->printers.begin();
      it != server_info->printers.end(); ++it) {
    if (it->printer_name == printer_name) {
      if (info)
        *info = *it;
      return true;
    }
  }
  return false;
}

PrintSystem::PrintServerWatcher*
PrintSystemCUPS::CreatePrintServerWatcher() {
  DCHECK(initialized_);
  return new PrintServerWatcherCUPS(this);
}

PrintSystem::PrinterWatcher* PrintSystemCUPS::CreatePrinterWatcher(
    const std::string& printer_name) {
  DCHECK(initialized_);
  DCHECK(!printer_name.empty());
  return new PrinterWatcherCUPS(this, printer_name);
}

PrintSystem::JobSpooler* PrintSystemCUPS::CreateJobSpooler() {
  DCHECK(initialized_);
  return new JobSpoolerCUPS(this);
}

std::string PrintSystemCUPS::GetSupportedMimeTypes() {
  // Since we hand off the document to the CUPS server directly, list some types
  // that we know CUPS supports (http://www.cups.org/articles.php?L205+TFAQ+Q)
  // TODO(sanjeevr): Determine this dynamically (http://crbug.com/73240).
  return
      "application/pdf,application/postscript,image/jpeg,image/png,image/gif";
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
  return new PrintSystemCUPS(print_system_settings);
}

int PrintSystemCUPS::PrintFile(const GURL& url, http_encryption_t encryption,
                               const char* name, const char* filename,
                               const char* title, int num_options,
                               cups_option_t* options) {
  if (url.is_empty()) {  // Use default (local) print server.
    return cupsPrintFile(name, filename, title, num_options, options);
  } else {
    printing::HttpConnectionCUPS http(url, encryption);
    http.SetBlocking(false);
    return cupsPrintFile2(http.http(), name, filename,
                          title, num_options, options);
  }
}

int PrintSystemCUPS::GetJobs(cups_job_t** jobs, const GURL& url,
                             http_encryption_t encryption,
                             const char* name, int myjobs, int whichjobs) {
  if (url.is_empty()) {  // Use default (local) print server.
    return cupsGetJobs(jobs, name, myjobs, whichjobs);
  } else {
    printing::HttpConnectionCUPS http(url, encryption);
    http.SetBlocking(false);
    return cupsGetJobs2(http.http(), jobs, name, myjobs, whichjobs);
  }
}

PlatformJobId PrintSystemCUPS::SpoolPrintJob(
    const std::string& print_ticket,
    const FilePath& print_data_file_path,
    const std::string& print_data_mime_type,
    const std::string& printer_name,
    const std::string& job_title,
    const std::vector<std::string>& tags,
    bool* dry_run) {
  DCHECK(initialized_);
  VLOG(1) << "CP_CUPS: Spooling print job for: " << printer_name;

  std::string short_printer_name;
  PrintServerInfoCUPS* server_info =
      FindServerByFullName(printer_name, &short_printer_name);
  if (!server_info)
    return false;

  child_process_logging::ScopedPrinterInfoSetter prn_info(
      server_info->backend->GetPrinterDriverInfo(printer_name));

  // We need to store options as char* string for the duration of the
  // cupsPrintFile2 call. We'll use map here to store options, since
  // Dictionary value from JSON parser returns wchat_t.
  std::map<std::string, std::string> options;
  bool res = ParsePrintTicket(print_ticket, &options);
  DCHECK(res);  // If print ticket is invalid we still print using defaults.

  // Check if this is a dry run (test) job.
  *dry_run = CloudPrintHelpers::IsDryRunJob(tags);
  if (*dry_run) {
    VLOG(1) << "CP_CUPS: Dry run job spooled.";
    return kDryRunJobId;
  }

  std::vector<cups_option_t> cups_options;
  std::map<std::string, std::string>::iterator it;

  for (it = options.begin(); it != options.end(); ++it) {
    cups_option_t opt;
    opt.name = const_cast<char*>(it->first.c_str());
    opt.value = const_cast<char*>(it->second.c_str());
    cups_options.push_back(opt);
  }

  int job_id = PrintFile(server_info->url,
                         cups_encryption_,
                         short_printer_name.c_str(),
                         print_data_file_path.value().c_str(),
                         job_title.c_str(),
                         cups_options.size(),
                         &(cups_options[0]));

  VLOG(1) << "CP_CUPS: Job spooled, id: " << job_id;

  return job_id;
}

std::string PrintSystemCUPS::MakeFullPrinterName(
    const GURL& url, const std::string& short_printer_name) {
  std::string full_name;
  full_name += "\\\\";
  full_name += url.host();
  if (!url.port().empty()) {
    full_name += ":";
    full_name += url.port();
  }
  full_name += "\\";
  full_name += short_printer_name;
  return full_name;
}

PrintServerInfoCUPS* PrintSystemCUPS::FindServerByFullName(
    const std::string& full_printer_name, std::string* short_printer_name) {
  size_t front = full_printer_name.find("\\\\");
  size_t separator = full_printer_name.find("\\", 2);
  if (front == std::string::npos || separator == std::string::npos) {
    LOG(WARNING) << "Invalid UNC printer name: " << full_printer_name;
    return NULL;
  }
  std::string server = full_printer_name.substr(2, separator - 2);

  PrintServerList::iterator it;
  for (it = print_servers_.begin(); it != print_servers_.end(); ++it) {
    std::string cur_server;
    cur_server += it->url.host();
    if (!it->url.port().empty()) {
      cur_server += ":";
      cur_server += it->url.port();
    }
    if (cur_server == server) {
      *short_printer_name = full_printer_name.substr(separator + 1);
      return &(*it);
    }
  }

  LOG(WARNING) << "Server not found for printer: " << full_printer_name;
  return NULL;
}

void PrintSystemCUPS::RunCapsCallback(
    const PrinterCapsAndDefaultsCallback& callback,
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& printer_info) {
  callback.Run(succeeded, printer_name, printer_info);
}

}  // namespace cloud_print
