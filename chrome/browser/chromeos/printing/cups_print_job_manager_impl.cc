// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_manager_impl.h"

#include <cups/cups.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager.h"
#include "chrome/browser/chromeos/printing/printer_pref_manager_factory.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "printing/backend/cups_connection.h"
#include "printing/printed_document.h"

namespace {

// The rate in milliseconds at which we will poll CUPS for print job updates.
const int kPollRate = 1000;

// Returns the equivalient CupsPrintJob#State from a CupsJob#JobState.
chromeos::CupsPrintJob::State ConvertState(printing::CupsJob::JobState state) {
  using cpj = chromeos::CupsPrintJob::State;

  switch (state) {
    case printing::CupsJob::PENDING:
      return cpj::STATE_WAITING;
    case printing::CupsJob::HELD:
      return cpj::STATE_SUSPENDED;
    case printing::CupsJob::PROCESSING:
      return cpj::STATE_STARTED;
    case printing::CupsJob::CANCELED:
      return cpj::STATE_CANCELLED;
    case printing::CupsJob::COMPLETED:
      return cpj::STATE_DOCUMENT_DONE;
    case printing::CupsJob::STOPPED:
      return cpj::STATE_SUSPENDED;
    case printing::CupsJob::ABORTED:
      return cpj::STATE_ERROR;
    case printing::CupsJob::UNKNOWN:
      break;
  }

  NOTREACHED();

  return cpj::STATE_NONE;
}

// Returns true if |state| represents a terminal state.
bool JobFinished(chromeos::CupsPrintJob::State state) {
  using chromeos::CupsPrintJob;
  return state == CupsPrintJob::State::STATE_CANCELLED ||
         state == CupsPrintJob::State::STATE_ERROR ||
         state == CupsPrintJob::State::STATE_DOCUMENT_DONE;
}

}  // namespace

namespace chromeos {

CupsPrintJobManagerImpl::CupsPrintJobManagerImpl(Profile* profile)
    : CupsPrintJobManager(profile),
      cups_connection_(GURL(), HTTP_ENCRYPT_NEVER, false),
      weak_ptr_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::NotificationService::AllSources());
}

CupsPrintJobManagerImpl::~CupsPrintJobManagerImpl() {}

bool CupsPrintJobManagerImpl::CancelPrintJob(CupsPrintJob* job) {
  std::string printer_id = job->printer().id();
  std::unique_ptr<::printing::CupsPrinter> printer =
      cups_connection_.GetPrinter(printer_id);
  if (!printer) {
    LOG(WARNING) << "Printer not found";
    return false;
  }

  return printer->CancelJob(job->job_id());
}

bool CupsPrintJobManagerImpl::SuspendPrintJob(CupsPrintJob* job) {
  NOTREACHED() << "Pause printer is not implemented";
  return false;
}

bool CupsPrintJobManagerImpl::ResumePrintJob(CupsPrintJob* job) {
  NOTREACHED() << "Resume printer is not implemented";
  return false;
}

void CupsPrintJobManagerImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);

  content::Details<::printing::JobEventDetails> job_details(details);

  // DOC_DONE occurs after the print job has been successfully sent to the
  // spooler which is when we begin tracking the print queue.
  if (job_details->type() == ::printing::JobEventDetails::DOC_DONE) {
    const ::printing::PrintedDocument* document = job_details->document();
    DCHECK(document);
    CreatePrintJob(base::UTF16ToUTF8(document->settings().device_name()),
                   base::UTF16ToUTF8(document->settings().title()),
                   document->page_count());
  }
}

bool CupsPrintJobManagerImpl::CreatePrintJob(const std::string& printer_name,
                                             const std::string& title,
                                             int total_page_number) {
  // Of the current jobs, find the new one for the printer.
  ::printing::CupsJob* new_job = nullptr;
  std::vector<::printing::CupsJob> cups_jobs = cups_connection_.GetJobs();
  for (auto& job : cups_jobs) {
    if (printer_name == job.printer_id &&
        !JobFinished(ConvertState(job.state)) &&
        !base::ContainsKey(jobs_,
                           CupsPrintJob::GetUniqueId(printer_name, job.id))) {
      // We found an untracked job.  It should be ours.
      new_job = &job;
      break;
    }
  }

  // The started job cannot be found in the queue.
  if (!new_job) {
    LOG(WARNING) << "Could not track print job.";
    return false;
  }

  auto printer =
      chromeos::PrinterPrefManagerFactory::GetForBrowserContext(profile_)
          ->GetPrinter(printer_name);
  if (!printer) {
    LOG(WARNING) << "Printer was removed while job was in progress.  It cannot "
                    "be tracked";
    return false;
  }

  // Create a new print job.
  auto cpj = base::MakeUnique<CupsPrintJob>(*printer, new_job->id, title,
                                            total_page_number);
  std::string key = cpj->GetUniqueId();
  jobs_[key] = std::move(cpj);
  NotifyJobCreated(jobs_[key].get());

  JobStateUpdated(jobs_[key].get(), ConvertState(new_job->state));

  ScheduleQuery();

  return true;
}

void CupsPrintJobManagerImpl::ScheduleQuery() {
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&CupsPrintJobManagerImpl::QueryCups,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollRate));
}

// Query CUPS asynchronously.  Post results back to UI thread.
void CupsPrintJobManagerImpl::QueryCups() {
  std::vector<::printing::CupsJob> jobs = cups_connection_.GetJobs();

  content::BrowserThread::PostTask(
      content::BrowserThread::ID::UI, FROM_HERE,
      base::Bind(&CupsPrintJobManagerImpl::UpdateJobs,
                 weak_ptr_factory_.GetWeakPtr(), jobs));
}

// Use job information to update local job states.  Update jobs that are no
// longer being reported on by CUPS.
void CupsPrintJobManagerImpl::UpdateJobs(
    const std::vector<::printing::CupsJob>& jobs) {
  std::set<std::string> updated_jobs;
  for (auto& job : jobs) {
    std::string key = CupsPrintJob::GetUniqueId(job.printer_id, job.id);
    if (!base::ContainsKey(jobs_, key)) {
      LOG(WARNING) << "Unexpected print job encountered";
      continue;
    }

    JobStateUpdated(jobs_[key].get(), ConvertState(job.state));
    updated_jobs.insert(key);
  }

  // Cleanup completed jobs.
  auto it = jobs_.begin();
  while (it != jobs_.end()) {
    auto& entry = *it;
    if (!base::ContainsKey(updated_jobs, entry.first)) {
      // We are no longer receiving updates for a job.  Declare it
      // complete.
      JobStateUpdated(entry.second.get(),
                      CupsPrintJob::State::STATE_DOCUMENT_DONE);
    }

    CupsPrintJob* job = entry.second.get();
    if (JobFinished(job->state())) {
      // Delete job since we will no longer receive events for it.
      it = jobs_.erase(it);
    } else {
      it++;
    }
  }

  if (!jobs_.empty())
    ScheduleQuery();
}

void CupsPrintJobManagerImpl::JobStateUpdated(CupsPrintJob* job,
                                              CupsPrintJob::State new_state) {
  if (job->state() == new_state)
    return;

  // We don't track state transitions because some of them might be missed due
  // to how we query jobs.
  job->set_state(new_state);
  switch (new_state) {
    case CupsPrintJob::State::STATE_NONE:
    case CupsPrintJob::State::STATE_WAITING:
      // States do not require notification.
      break;
    case CupsPrintJob::State::STATE_STARTED:
      NotifyJobStarted(job);
      break;
    case CupsPrintJob::State::STATE_RESUMED:
      NotifyJobResumed(job);
      break;
    case CupsPrintJob::State::STATE_SUSPENDED:
      NotifyJobSuspended(job);
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
      NotifyJobCanceled(job);
      break;
    case CupsPrintJob::State::STATE_ERROR:
      NotifyJobError(job);
      break;
    case CupsPrintJob::State::STATE_PAGE_DONE:
      NOTREACHED() << "CUPS does not surface this state so it's not expected";
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      NotifyJobDone(job);
      break;
  }
}

// static
CupsPrintJobManager* CupsPrintJobManager::CreateInstance(Profile* profile) {
  return new CupsPrintJobManagerImpl(profile);
}

}  // namespace chromeos
