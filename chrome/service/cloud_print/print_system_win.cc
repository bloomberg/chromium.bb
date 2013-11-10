// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system_win.h"

#include "base/strings/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "chrome/common/crash_keys.h"
#include "printing/backend/win_helper.h"

namespace cloud_print {

namespace {

class PrintSystemWatcherWin : public base::win::ObjectWatcher::Delegate {
 public:
  PrintSystemWatcherWin()
      : delegate_(NULL),
        did_signal_(false) {
  }
  ~PrintSystemWatcherWin() {
    Stop();
  }

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnPrinterAdded() = 0;
    virtual void OnPrinterDeleted() = 0;
    virtual void OnPrinterChanged() = 0;
    virtual void OnJobChanged() = 0;
  };

  bool Start(const std::string& printer_name, Delegate* delegate) {
    scoped_refptr<printing::PrintBackend> print_backend(
        printing::PrintBackend::CreateInstance(NULL));
    printer_info_ = print_backend->GetPrinterDriverInfo(printer_name);
    crash_keys::ScopedPrinterInfo crash_key(printer_info_);

    delegate_ = delegate;
    // An empty printer name means watch the current server, we need to pass
    // NULL to OpenPrinter.
    LPTSTR printer_name_to_use = NULL;
    std::wstring printer_name_wide;
    if (!printer_name.empty()) {
      printer_name_wide = UTF8ToWide(printer_name);
      printer_name_to_use = const_cast<LPTSTR>(printer_name_wide.c_str());
    }
    bool ret = false;
    if (printer_.OpenPrinter(printer_name_to_use)) {
      printer_change_.Set(FindFirstPrinterChangeNotification(
          printer_, PRINTER_CHANGE_PRINTER|PRINTER_CHANGE_JOB, 0, NULL));
      if (printer_change_.IsValid()) {
        ret = watcher_.StartWatching(printer_change_, this);
      }
    }
    if (!ret) {
      Stop();
    }
    return ret;
  }

  bool Stop() {
    watcher_.StopWatching();
    printer_.Close();
    printer_change_.Close();
    return true;
  }

  // base::ObjectWatcher::Delegate method
  virtual void OnObjectSignaled(HANDLE object) {
    crash_keys::ScopedPrinterInfo crash_key(printer_info_);
    DWORD change = 0;
    FindNextPrinterChangeNotification(object, &change, NULL, NULL);

    if (change != ((PRINTER_CHANGE_PRINTER|PRINTER_CHANGE_JOB) &
                  (~PRINTER_CHANGE_FAILED_CONNECTION_PRINTER))) {
      // For printer connections, we get spurious change notifications with
      // all flags set except PRINTER_CHANGE_FAILED_CONNECTION_PRINTER.
      // Ignore these.
      if (change & PRINTER_CHANGE_ADD_PRINTER) {
        delegate_->OnPrinterAdded();
      } else if (change & PRINTER_CHANGE_DELETE_PRINTER) {
        delegate_->OnPrinterDeleted();
      } else if (change & PRINTER_CHANGE_SET_PRINTER) {
        delegate_->OnPrinterChanged();
      }
      if (change & PRINTER_CHANGE_JOB) {
        delegate_->OnJobChanged();
      }
    }
    watcher_.StartWatching(printer_change_, this);
  }

  bool GetCurrentPrinterInfo(printing::PrinterBasicInfo* printer_info) {
    DCHECK(printer_info);
    return InitBasicPrinterInfo(printer_, printer_info);
  }

 private:
  base::win::ObjectWatcher watcher_;
  printing::ScopedPrinterHandle printer_;  // The printer being watched
  // Returned by FindFirstPrinterChangeNotifier.
  printing::ScopedPrinterChangeHandle printer_change_;
  Delegate* delegate_;           // Delegate to notify
  bool did_signal_;              // DoneWaiting was called
  std::string printer_info_;     // For crash reporting.
};

class PrintServerWatcherWin
  : public PrintSystem::PrintServerWatcher,
    public PrintSystemWatcherWin::Delegate {
 public:
  PrintServerWatcherWin() : delegate_(NULL) {}

  // PrintSystem::PrintServerWatcher implementation.
  virtual bool StartWatching(
      PrintSystem::PrintServerWatcher::Delegate* delegate) OVERRIDE{
    delegate_ = delegate;
    return watcher_.Start(std::string(), this);
  }

  virtual bool StopWatching() OVERRIDE{
    bool ret = watcher_.Stop();
    delegate_ = NULL;
    return ret;
  }

  // PrintSystemWatcherWin::Delegate implementation.
  virtual void OnPrinterAdded() OVERRIDE {
    delegate_->OnPrinterAdded();
  }
  virtual void OnPrinterDeleted() OVERRIDE {}
  virtual void OnPrinterChanged() OVERRIDE {}
  virtual void OnJobChanged() OVERRIDE {}

  protected:
  virtual ~PrintServerWatcherWin() {}

 private:
  PrintSystem::PrintServerWatcher::Delegate* delegate_;
  PrintSystemWatcherWin watcher_;

  DISALLOW_COPY_AND_ASSIGN(PrintServerWatcherWin);
};

class PrinterWatcherWin
    : public PrintSystem::PrinterWatcher,
      public PrintSystemWatcherWin::Delegate {
 public:
  explicit PrinterWatcherWin(const std::string& printer_name)
      : printer_name_(printer_name),
        delegate_(NULL) {
  }

  // PrintSystem::PrinterWatcher implementation.
  virtual bool StartWatching(
      PrintSystem::PrinterWatcher::Delegate* delegate) OVERRIDE {
    delegate_ = delegate;
    return watcher_.Start(printer_name_, this);
  }

  virtual bool StopWatching() OVERRIDE {
    bool ret = watcher_.Stop();
    delegate_ = NULL;
    return ret;
  }

  virtual bool GetCurrentPrinterInfo(
      printing::PrinterBasicInfo* printer_info) OVERRIDE {
    return watcher_.GetCurrentPrinterInfo(printer_info);
  }

  // PrintSystemWatcherWin::Delegate implementation.
  virtual void OnPrinterAdded() OVERRIDE {
    NOTREACHED();
  }
  virtual void OnPrinterDeleted() OVERRIDE {
    delegate_->OnPrinterDeleted();
  }
  virtual void OnPrinterChanged() OVERRIDE {
    delegate_->OnPrinterChanged();
  }
  virtual void OnJobChanged() OVERRIDE {
    delegate_->OnJobChanged();
  }

  protected:
  virtual ~PrinterWatcherWin() {}

 private:
  std::string printer_name_;
  PrintSystem::PrinterWatcher::Delegate* delegate_;
  PrintSystemWatcherWin watcher_;

  DISALLOW_COPY_AND_ASSIGN(PrinterWatcherWin);
};

}  // namespace

PrintSystemWin::PrintSystemWin() {
  print_backend_ = printing::PrintBackend::CreateInstance(NULL);
}

PrintSystemWin::~PrintSystemWin() {
}

PrintSystem::PrintSystemResult PrintSystemWin::EnumeratePrinters(
    printing::PrinterList* printer_list) {
  bool ret = print_backend_->EnumeratePrinters(printer_list);
  return PrintSystemResult(ret, std::string());
}

bool PrintSystemWin::IsValidPrinter(const std::string& printer_name) {
  return print_backend_->IsValidPrinter(printer_name);
}

bool PrintSystemWin::GetJobDetails(const std::string& printer_name,
                                   PlatformJobId job_id,
                                   PrintJobDetails *job_details) {
  crash_keys::ScopedPrinterInfo crash_key(
      print_backend_->GetPrinterDriverInfo(printer_name));
  DCHECK(job_details);
  printing::ScopedPrinterHandle printer_handle;
  std::wstring printer_name_wide = UTF8ToWide(printer_name);
  printer_handle.OpenPrinter(printer_name_wide.c_str());
  DCHECK(printer_handle.IsValid());
  bool ret = false;
  if (printer_handle.IsValid()) {
    DWORD bytes_needed = 0;
    GetJob(printer_handle, job_id, 1, NULL, 0, &bytes_needed);
    DWORD last_error = GetLastError();
    if (ERROR_INVALID_PARAMETER != last_error) {
      // ERROR_INVALID_PARAMETER normally means that the job id is not valid.
      DCHECK(last_error == ERROR_INSUFFICIENT_BUFFER);
      scoped_ptr<BYTE[]> job_info_buffer(new BYTE[bytes_needed]);
      if (GetJob(printer_handle, job_id, 1, job_info_buffer.get(), bytes_needed,
                &bytes_needed)) {
        JOB_INFO_1 *job_info =
            reinterpret_cast<JOB_INFO_1 *>(job_info_buffer.get());
        if (job_info->pStatus) {
          WideToUTF8(job_info->pStatus, wcslen(job_info->pStatus),
                     &job_details->status_message);
        }
        job_details->platform_status_flags = job_info->Status;
        if ((job_info->Status & JOB_STATUS_COMPLETE) ||
            (job_info->Status & JOB_STATUS_PRINTED)) {
          job_details->status = PRINT_JOB_STATUS_COMPLETED;
        } else if (job_info->Status & JOB_STATUS_ERROR) {
          job_details->status = PRINT_JOB_STATUS_ERROR;
        } else {
          job_details->status = PRINT_JOB_STATUS_IN_PROGRESS;
        }
        job_details->total_pages = job_info->TotalPages;
        job_details->pages_printed = job_info->PagesPrinted;
        ret = true;
      }
    }
  }
  return ret;
}

PrintSystem::PrintServerWatcher*
PrintSystemWin::CreatePrintServerWatcher() {
  return new PrintServerWatcherWin();
}

PrintSystem::PrinterWatcher* PrintSystemWin::CreatePrinterWatcher(
    const std::string& printer_name) {
  DCHECK(!printer_name.empty());
  return new PrinterWatcherWin(printer_name);
}

std::string PrintSystemWin::GetPrinterDriverInfo(
    const std::string& printer_name) const {
  return print_backend_->GetPrinterDriverInfo(printer_name);
}

}  // namespace cloud_print
