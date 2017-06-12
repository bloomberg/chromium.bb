// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_manager_impl.h"

#include <cups/cups.h>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/printers_manager.h"
#include "chrome/browser/chromeos/printing/printers_manager_factory.h"
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

// How long we'll wait to connect to a printer before declaring an error.
const int kConnectingTimeout = 20;

// Threshold for giving up on communicating with CUPS.
const int kRetryMax = 6;

using State = chromeos::CupsPrintJob::State;
using ErrorCode = chromeos::CupsPrintJob::ErrorCode;

using PrinterReason = printing::PrinterStatus::PrinterReason;

// Enumeration of print job results for histograms.  Do not modify!
enum JobResultForHistogram {
  UNKNOWN = 0,         // unidentified result
  FINISHED = 1,        // successful completion of job
  TIMEOUT_CANCEL = 2,  // cancelled due to timeout
  PRINTER_CANCEL = 3,  // cancelled by printer
  LOST = 4,            // final state never received
  RESULT_MAX
};

// Returns the appropriate JobResultForHistogram for a given |state|.  Only
// FINISHED and PRINTER_CANCEL are derived from CupsPrintJob::State.
JobResultForHistogram ResultForHistogram(State state) {
  switch (state) {
    case State::STATE_DOCUMENT_DONE:
      return FINISHED;
    case State::STATE_CANCELLED:
      return PRINTER_CANCEL;
    default:
      break;
  }

  return UNKNOWN;
}

void RecordJobResult(JobResultForHistogram result) {
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.JobResult", result, RESULT_MAX);
}

// Returns the equivalient CupsPrintJob#State from a CupsJob#JobState.
chromeos::CupsPrintJob::State ConvertState(printing::CupsJob::JobState state) {
  switch (state) {
    case printing::CupsJob::PENDING:
      return State::STATE_WAITING;
    case printing::CupsJob::HELD:
      return State::STATE_SUSPENDED;
    case printing::CupsJob::PROCESSING:
      return State::STATE_STARTED;
    case printing::CupsJob::CANCELED:
      return State::STATE_CANCELLED;
    case printing::CupsJob::COMPLETED:
      return State::STATE_DOCUMENT_DONE;
    case printing::CupsJob::STOPPED:
      return State::STATE_SUSPENDED;
    case printing::CupsJob::ABORTED:
      return State::STATE_ERROR;
    case printing::CupsJob::UNKNOWN:
      break;
  }

  NOTREACHED();

  return State::STATE_NONE;
}

chromeos::QueryResult QueryCups(::printing::CupsConnection* connection,
                                const std::vector<std::string>& printer_ids) {
  chromeos::QueryResult result;
  result.success = connection->GetJobs(printer_ids, &result.queues);
  return result;
}

// Returns true if |printer_status|.reasons contains |reason|.
bool ContainsReason(const printing::PrinterStatus printer_status,
                    PrinterReason::Reason reason) {
  return std::find_if(printer_status.reasons.begin(),
                      printer_status.reasons.end(),
                      [&reason](const PrinterReason& r) {
                        return r.reason == reason;
                      }) != printer_status.reasons.end();
}

// Extracts an ErrorCode from PrinterStatus#reasons.  Returns NO_ERROR if there
// are no reasons which indicate an error.
chromeos::CupsPrintJob::ErrorCode ErrorCodeFromReasons(
    const printing::PrinterStatus& printer_status) {
  for (const auto& reason : printer_status.reasons) {
    switch (reason.reason) {
      case PrinterReason::MEDIA_JAM:
      case PrinterReason::MEDIA_EMPTY:
      case PrinterReason::MEDIA_NEEDED:
      case PrinterReason::MEDIA_LOW:
        return chromeos::CupsPrintJob::ErrorCode::PAPER_JAM;
      case PrinterReason::TONER_EMPTY:
      case PrinterReason::TONER_LOW:
        return chromeos::CupsPrintJob::ErrorCode::OUT_OF_INK;
      default:
        break;
    }
  }
  return chromeos::CupsPrintJob::ErrorCode::NO_ERROR;
}

// Check if the job should timeout.  Returns true if the job has timed out.
bool EnforceTimeout(const printing::CupsJob& job,
                    chromeos::CupsPrintJob* print_job) {
  // Check to see if we should time out.
  base::TimeDelta time_waiting =
      base::Time::Now() - base::Time::FromTimeT(job.processing_started);
  if (time_waiting > base::TimeDelta::FromSeconds(kConnectingTimeout)) {
    print_job->set_state(chromeos::CupsPrintJob::State::STATE_ERROR);
    print_job->set_error_code(
        chromeos::CupsPrintJob::ErrorCode::PRINTER_UNREACHABLE);
    return true;
  }

  return false;
}

// Update the current printed page.  Returns true of the page has been updated.
bool UpdateCurrentPage(const printing::CupsJob& job,
                       chromeos::CupsPrintJob* print_job) {
  bool pages_updated = false;
  if (job.current_pages <= 0) {
    print_job->set_printed_page_number(0);
    print_job->set_state(State::STATE_STARTED);
  } else {
    pages_updated = job.current_pages != print_job->printed_page_number();
    print_job->set_printed_page_number(job.current_pages);
    print_job->set_state(State::STATE_PAGE_DONE);
  }

  return pages_updated;
}

}  // namespace

namespace chromeos {

QueryResult::QueryResult() = default;

QueryResult::QueryResult(const QueryResult& other) = default;

QueryResult::~QueryResult() = default;

CupsPrintJobManagerImpl::CupsPrintJobManagerImpl(Profile* profile)
    : CupsPrintJobManager(profile),
      cups_connection_(GURL(), HTTP_ENCRYPT_NEVER, false),
      weak_ptr_factory_(this) {
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::NotificationService::AllSources());
}

CupsPrintJobManagerImpl::~CupsPrintJobManagerImpl() {}

// Must be run from the UI thread.
void CupsPrintJobManagerImpl::CancelPrintJob(CupsPrintJob* job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Copy job_id and printer_id.  |job| is about to be freed.
  const int job_id = job->job_id();
  const std::string printer_id = job->printer().id();

  // Stop montioring jobs after we cancel them.  The user no longer cares.
  jobs_.erase(job->GetUniqueId());

  // Be sure to copy out all relevant fields.  |job| may be destroyed after we
  // exit this scope.
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&CupsPrintJobManagerImpl::CancelJobImpl,
                 weak_ptr_factory_.GetWeakPtr(), printer_id, job_id));
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
                   job_details->job_id(), document->page_count());
  }
}

bool CupsPrintJobManagerImpl::CreatePrintJob(const std::string& printer_name,
                                             const std::string& title,
                                             int job_id,
                                             int total_page_number) {
  auto printer =
      PrintersManagerFactory::GetForBrowserContext(profile_)->GetPrinter(
          printer_name);
  if (!printer) {
    LOG(WARNING) << "Printer was removed while job was in progress.  It cannot "
                    "be tracked";
    return false;
  }

  // Records the number of jobs we're currently tracking when a new job is
  // started.  This is equivalent to print queue size in the current
  // implementation.
  UMA_HISTOGRAM_EXACT_LINEAR("Printing.CUPS.PrintJobsQueued", jobs_.size(), 20);

  // Create a new print job.
  auto cpj = base::MakeUnique<CupsPrintJob>(*printer, job_id, title,
                                            total_page_number);
  std::string key = cpj->GetUniqueId();
  jobs_[key] = std::move(cpj);

  CupsPrintJob* job = jobs_[key].get();
  NotifyJobCreated(job);

  // Always start jobs in the waiting state.
  job->set_state(CupsPrintJob::State::STATE_WAITING);
  NotifyJobUpdated(job);

  ScheduleQuery(base::TimeDelta());

  return true;
}

void CupsPrintJobManagerImpl::ScheduleQuery() {
  ScheduleQuery(base::TimeDelta::FromMilliseconds(kPollRate));
}

void CupsPrintJobManagerImpl::ScheduleQuery(const base::TimeDelta& delay) {
  if (!in_query_) {
    in_query_ = true;

    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CupsPrintJobManagerImpl::PostQuery,
                   weak_ptr_factory_.GetWeakPtr()),
        delay);
  }
}

void CupsPrintJobManagerImpl::PostQuery() {
  // The set of active printers is expected to be small.
  std::set<std::string> printer_ids;
  for (const auto& entry : jobs_) {
    printer_ids.insert(entry.second->printer().id());
  }
  std::vector<std::string> ids{printer_ids.begin(), printer_ids.end()};

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&QueryCups, &cups_connection_, ids),
      base::Bind(&CupsPrintJobManagerImpl::UpdateJobs,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool CupsPrintJobManagerImpl::UpdatePrintJob(
    const ::printing::PrinterStatus& printer_status,
    const ::printing::CupsJob& job,
    CupsPrintJob* print_job) {
  DCHECK_EQ(job.id, print_job->job_id());

  State old_state = print_job->state();

  bool pages_updated = false;
  switch (job.state) {
    case ::printing::CupsJob::PROCESSING:
      if (ContainsReason(printer_status, PrinterReason::CONNECTING_TO_DEVICE)) {
        if (EnforceTimeout(job, print_job)) {
          LOG(WARNING) << "Connecting to printer timed out";
          print_job->set_expired(true);
        }
      } else {
        pages_updated = UpdateCurrentPage(job, print_job);
      }
      break;
    case ::printing::CupsJob::COMPLETED:
      DCHECK_GE(job.current_pages, print_job->total_page_number());
      print_job->set_state(State::STATE_DOCUMENT_DONE);
      break;
    case ::printing::CupsJob::ABORTED:
    case ::printing::CupsJob::CANCELED:
      print_job->set_error_code(ErrorCodeFromReasons(printer_status));
    // fall through
    default:
      print_job->set_state(ConvertState(job.state));
      break;
  }

  return print_job->state() != old_state || pages_updated;
}

// Use job information to update local job states.  Previously completed jobs
// could be in |jobs| but those are ignored as we will not emit updates for them
// after they are completed.
void CupsPrintJobManagerImpl::UpdateJobs(const QueryResult& result) {
  const std::vector<::printing::QueueStatus>& queues = result.queues;

  // Query has completed.  Allow more queries.
  in_query_ = false;

  // If the query failed, either retry or purge.
  if (!result.success) {
    retry_count_++;
    LOG(WARNING) << "Failed to query CUPS for queue status.  Schedule retry ("
                 << retry_count_ << ")";
    if (retry_count_ > kRetryMax) {
      LOG(ERROR) << "CUPS is unreachable.  Giving up on all jobs.";
      PurgeJobs();
    } else {
      // Schedule another query with a larger delay.
      DCHECK_GE(1, retry_count_);
      ScheduleQuery(
          base::TimeDelta::FromMilliseconds(kPollRate * retry_count_));
    }
    return;
  }

  // A query has completed.  Reset retry counter.
  retry_count_ = 0;

  std::vector<std::string> active_jobs;
  for (const auto& queue : queues) {
    for (auto& job : queue.jobs) {
      std::string key = CupsPrintJob::GetUniqueId(job.printer_id, job.id);
      const auto& entry = jobs_.find(key);
      if (entry == jobs_.end())
        continue;

      CupsPrintJob* print_job = entry->second.get();

      if (UpdatePrintJob(queue.printer_status, job, print_job)) {
        // The state of the job changed, notify observers.
        NotifyJobStateUpdate(print_job);
      }

      if (print_job->expired()) {
        // Job needs to be forcibly cancelled.
        RecordJobResult(TIMEOUT_CANCEL);
        CancelPrintJob(print_job);
        // Beware, print_job was removed from jobs_ and deleted.
      } else if (print_job->IsJobFinished()) {
        // Cleanup completed jobs.
        RecordJobResult(ResultForHistogram(print_job->state()));
        jobs_.erase(entry);
      } else {
        active_jobs.push_back(key);
      }
    }
  }

  // Keep polling until all jobs complete or error.
  if (!active_jobs.empty()) {
    // During normal operations, we poll at the default rate.
    ScheduleQuery();
  } else if (!jobs_.empty()) {
    // We're tracking jobs that we didn't receive an update for.  Something bad
    // has happened.
    LOG(ERROR) << "Lost track of (" << jobs_.size() << ") jobs";
    PurgeJobs();
  }
}

void CupsPrintJobManagerImpl::PurgeJobs() {
  for (const auto& entry : jobs_) {
    // Declare all lost jobs errors.
    RecordJobResult(LOST);
    CupsPrintJob* job = entry.second.get();
    job->set_state(CupsPrintJob::State::STATE_ERROR);
    NotifyJobStateUpdate(job);
  }

  jobs_.clear();
}

void CupsPrintJobManagerImpl::CancelJobImpl(const std::string& printer_id,
                                            const int job_id) {
  std::unique_ptr<::printing::CupsPrinter> printer =
      cups_connection_.GetPrinter(printer_id);
  if (!printer) {
    LOG(WARNING) << "Printer not found: " << printer_id;
    return;
  }

  if (!printer->CancelJob(job_id)) {
    // This is not expected to fail but log it if it does.
    LOG(WARNING) << "Cancelling job failed.  Job may be stuck in queue.";
  }
}

void CupsPrintJobManagerImpl::NotifyJobStateUpdate(CupsPrintJob* job) {
  switch (job->state()) {
    case State::STATE_NONE:
      // State does not require notification.
      break;
    case State::STATE_WAITING:
      NotifyJobUpdated(job);
      break;
    case State::STATE_STARTED:
      NotifyJobStarted(job);
      break;
    case State::STATE_PAGE_DONE:
      NotifyJobUpdated(job);
      break;
    case State::STATE_RESUMED:
      NotifyJobResumed(job);
      break;
    case State::STATE_SUSPENDED:
      NotifyJobSuspended(job);
      break;
    case State::STATE_CANCELLED:
      NotifyJobCanceled(job);
      break;
    case State::STATE_ERROR:
      NotifyJobError(job);
      break;
    case State::STATE_DOCUMENT_DONE:
      NotifyJobDone(job);
      break;
  }
}

// static
CupsPrintJobManager* CupsPrintJobManager::CreateInstance(Profile* profile) {
  return new CupsPrintJobManagerImpl(profile);
}

}  // namespace chromeos
