// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "printing/printed_document.h"
#include "printing/printed_page.h"

namespace printing {

PrintJobManager::PrintJobManager() {
  registrar_.Add(this, NotificationType::PRINT_JOB_EVENT,
                 NotificationService::AllSources());
}

PrintJobManager::~PrintJobManager() {
  base::AutoLock lock(lock_);
  queued_queries_.clear();
}

void PrintJobManager::OnQuit() {
#if defined(OS_MACOSX)
  // OnQuit is too late to try to wait for jobs on the Mac, since the runloop
  // has already been torn down; instead, StopJobs(true) is called earlier in
  // the shutdown process, and this is just here in case something sneaks
  // in after that.
  StopJobs(false);
#else
  StopJobs(true);
#endif
  registrar_.RemoveAll();
}

void PrintJobManager::StopJobs(bool wait_for_finish) {
  if (current_jobs_.empty())
    return;
  {
    // Copy the array since it can be modified in transit.
    PrintJobs current_jobs(current_jobs_);
    // Wait for each job to finish.
    for (size_t i = 0; i < current_jobs.size(); ++i) {
      PrintJob* job = current_jobs[i];
      if (!job)
        continue;
      // Wait for 120 seconds for the print job to be spooled.
      if (wait_for_finish)
        job->FlushJob(120000);
      job->Stop();
    }
  }
  current_jobs_.clear();
}

void PrintJobManager::QueuePrinterQuery(PrinterQuery* job) {
  base::AutoLock lock(lock_);
  DCHECK(job);
  queued_queries_.push_back(make_scoped_refptr(job));
  DCHECK(job->is_valid());
}

void PrintJobManager::PopPrinterQuery(int document_cookie,
                                      scoped_refptr<PrinterQuery>* job) {
  base::AutoLock lock(lock_);
  for (PrinterQueries::iterator itr = queued_queries_.begin();
       itr != queued_queries_.end();
       ++itr) {
    PrinterQuery* current_query = *itr;
    if (current_query->cookie() == document_cookie &&
        !current_query->is_callback_pending()) {
      *job = current_query;
      queued_queries_.erase(itr);
      DCHECK(current_query->is_valid());
      return;
    }
  }
}


void PrintJobManager::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PRINT_JOB_EVENT: {
      OnPrintJobEvent(Source<PrintJob>(source).ptr(),
                      *Details<JobEventDetails>(details).ptr());
      break;
    }
    case NotificationType::PREF_CHANGED: {
      const std::string* pref_name = Details<std::string>(details).ptr();
      if (*pref_name == prefs::kPrintingEnabled) {
        PrefService *local_state = g_browser_process->local_state();
        set_printing_enabled(local_state->GetBoolean(prefs::kPrintingEnabled));
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintJobManager::OnPrintJobEvent(
    PrintJob* print_job,
    const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::NEW_DOC: {
      DCHECK(current_jobs_.end() == std::find(current_jobs_.begin(),
                                              current_jobs_.end(),
                                              print_job));
      // Causes a AddRef().
      current_jobs_.push_back(make_scoped_refptr(print_job));
      break;
    }
    case JobEventDetails::JOB_DONE: {
      PrintJobs::iterator itr = std::find(current_jobs_.begin(),
                                          current_jobs_.end(),
                                          print_job);
      DCHECK(current_jobs_.end() != itr);
      current_jobs_.erase(itr);
      DCHECK(current_jobs_.end() == std::find(current_jobs_.begin(),
                                              current_jobs_.end(),
                                              print_job));
      break;
    }
    case JobEventDetails::FAILED: {
      PrintJobs::iterator itr = std::find(current_jobs_.begin(),
                                          current_jobs_.end(),
                                          print_job);
      // A failed job may have never started.
      if (current_jobs_.end() != itr) {
        current_jobs_.erase(itr);
        DCHECK(current_jobs_.end() ==
                  std::find(current_jobs_.begin(),
                            current_jobs_.end(),
                            print_job));
      }
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::NEW_PAGE:
    case JobEventDetails::PAGE_DONE:
    case JobEventDetails::DOC_DONE:
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      // Don't care.
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

bool PrintJobManager::printing_enabled() {
  base::AutoLock lock(enabled_lock_);
  return printing_enabled_;
}

void PrintJobManager::set_printing_enabled(bool printing_enabled) {
  base::AutoLock lock(enabled_lock_);
  printing_enabled_ = printing_enabled;
}

}  // namespace printing
