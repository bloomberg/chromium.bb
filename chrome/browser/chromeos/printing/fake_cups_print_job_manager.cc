// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/fake_cups_print_job_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

// static
int FakeCupsPrintJobManager::next_job_id_ = 0;

// static
CupsPrintJobManager* CupsPrintJobManager::Get(
    content::BrowserContext* browser_context) {
  return CupsPrintJobManagerFactory::GetForBrowserContext(browser_context);
}

FakeCupsPrintJobManager::FakeCupsPrintJobManager(Profile* profile)
    : CupsPrintJobManager(profile), weak_ptr_factory_(this) {}

FakeCupsPrintJobManager::~FakeCupsPrintJobManager() {}

bool FakeCupsPrintJobManager::CreatePrintJob(const std::string& printer_name,
                                             const std::string& title,
                                             int total_page_number) {
  Printer printer(printer_name);
  printer.set_display_name(printer_name);
  // Create a new print job.
  std::unique_ptr<CupsPrintJob> new_job = base::MakeUnique<CupsPrintJob>(
      printer, next_job_id_++, title, total_page_number);
  print_jobs_.push_back(std::move(new_job));

  // Show the waiting-for-printing notification immediately.
  base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
      FROM_HERE,
      base::Bind(&FakeCupsPrintJobManager::ChangePrintJobState,
                 weak_ptr_factory_.GetWeakPtr(), print_jobs_.back().get()),
      base::TimeDelta());

  return true;
}

bool FakeCupsPrintJobManager::CancelPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_CANCELLED);
  for (auto observer : observers_)
    observer->OnPrintJobCancelled(job->job_id());

  // Note: |job| is deleted here.
  for (auto iter = print_jobs_.begin(); iter != print_jobs_.end(); ++iter) {
    if (iter->get() == job) {
      print_jobs_.erase(iter);
      break;
    }
  }

  return true;
}

bool FakeCupsPrintJobManager::SuspendPrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_SUSPENDED);
  for (auto observer : observers_)
    observer->OnPrintJobSuspended(job);
  return true;
}

bool FakeCupsPrintJobManager::ResumePrintJob(CupsPrintJob* job) {
  job->set_state(CupsPrintJob::State::STATE_RESUMED);
  for (auto observer : observers_)
    observer->OnPrintJobResumed(job);

  base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
      FROM_HERE, base::Bind(&FakeCupsPrintJobManager::ChangePrintJobState,
                            weak_ptr_factory_.GetWeakPtr(), job),
      base::TimeDelta::FromMilliseconds(3000));

  return true;
}

void FakeCupsPrintJobManager::ChangePrintJobState(CupsPrintJob* job) {
  // |job| might have been deleted.
  bool found = false;
  for (auto iter = print_jobs_.begin(); iter != print_jobs_.end(); ++iter) {
    if (iter->get() == job) {
      found = true;
      break;
    }
  }

  if (!found || job->state() == CupsPrintJob::State::STATE_SUSPENDED ||
      job->state() == CupsPrintJob::State::STATE_ERROR) {
    return;
  }

  switch (job->state()) {
    case CupsPrintJob::State::STATE_NONE:
      job->set_state(CupsPrintJob::State::STATE_WAITING);
      for (auto observer : observers_)
        observer->OnPrintJobCreated(job);
      break;
    case CupsPrintJob::State::STATE_WAITING:
      job->set_state(CupsPrintJob::State::STATE_STARTED);
      for (auto observer : observers_)
        observer->OnPrintJobStarted(job);
      break;
    case CupsPrintJob::State::STATE_STARTED:
      job->set_printed_page_number(job->printed_page_number() + 1);
      job->set_state(CupsPrintJob::State::STATE_PAGE_DONE);
      for (auto observer : observers_)
        observer->OnPrintJobUpdated(job);
      break;
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_RESUMED:
      if (job->printed_page_number() == job->total_page_number()) {
        job->set_state(CupsPrintJob::State::STATE_DOCUMENT_DONE);
        for (auto observer : observers_)
          observer->OnPrintJobDone(job);
      } else {
        job->set_printed_page_number(job->printed_page_number() + 1);
        job->set_state(CupsPrintJob::State::STATE_PAGE_DONE);
        for (auto observer : observers_)
          observer->OnPrintJobUpdated(job);
      }
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      // Only for testing
      job->set_state(CupsPrintJob::State::STATE_ERROR);
      job->set_error_code(CupsPrintJob::ErrorCode::UNKNOWN_ERROR);
      for (auto observer : observers_)
        observer->OnPrintJobError(job);
      break;
    default:
      break;
  }

  base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
      FROM_HERE, base::Bind(&FakeCupsPrintJobManager::ChangePrintJobState,
                            weak_ptr_factory_.GetWeakPtr(), job),
      base::TimeDelta::FromMilliseconds(3000));
}

}  // namespace chromeos
