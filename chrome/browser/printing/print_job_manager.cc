// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_manager.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "printing/printed_document.h"
#include "printing/printed_page.h"

using content::BrowserThread;

namespace printing {

PrintJobManager::PrintJobManager() {
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::NotificationService::AllSources());
}

PrintJobManager::~PrintJobManager() {
  base::AutoLock lock(lock_);
  queued_queries_.clear();
}

void PrintJobManager::InitOnUIThread(PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  printing_enabled_.Init(prefs::kPrintingEnabled, prefs, NULL);
  printing_enabled_.MoveToThread(BrowserThread::IO);
}

void PrintJobManager::OnQuit() {
  StopJobs(true);
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
      // Wait for two minutes for the print job to be spooled.
      if (wait_for_finish)
        job->FlushJob(base::TimeDelta::FromMinutes(2));
      job->Stop();
    }
  }
  current_jobs_.clear();
}

void PrintJobManager::SetPrintDestination(
    PrintDestinationInterface* destination) {
  destination_ = destination;
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

// static
void PrintJobManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kPrintingEnabled, true);
}

void PrintJobManager::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PRINT_JOB_EVENT: {
      OnPrintJobEvent(content::Source<PrintJob>(source).ptr(),
                      *content::Details<JobEventDetails>(details).ptr());
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
      destination_ = NULL;
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

bool PrintJobManager::printing_enabled() const {
  return *printing_enabled_;
}

}  // namespace printing
