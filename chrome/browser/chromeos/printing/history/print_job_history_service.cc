// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/history/print_job_info_conversions.h"

namespace chromeos {

namespace {

// This callback is used in unit tests to wait until print job is saved in the
// database and check whether the operation is successful.
base::RepeatingCallback<void(bool success)>*
    g_test_on_print_job_saved_callback = nullptr;

}  // namespace

PrintJobHistoryService::PrintJobHistoryService(
    std::unique_ptr<PrintJobDatabase> print_job_database,
    CupsPrintJobManager* print_job_manager)
    : print_job_database_(std::move(print_job_database)),
      print_job_manager_(print_job_manager) {
  DCHECK(print_job_manager_);
  print_job_manager_->AddObserver(this);
  print_job_database_->Initialize(base::DoNothing());
}

PrintJobHistoryService::~PrintJobHistoryService() {
  DCHECK(print_job_manager_);
  print_job_manager_->RemoveObserver(this);
}

void PrintJobHistoryService::GetPrintJobs(
    PrintJobDatabase::GetPrintJobsCallback callback) {
  print_job_database_->GetPrintJobs(std::move(callback));
}

void PrintJobHistoryService::SetOnPrintJobSavedCallbackForTesting(
    base::RepeatingCallback<void(bool success)>* callback) {
  g_test_on_print_job_saved_callback = callback;
}

void PrintJobHistoryService::OnPrintJobDone(base::WeakPtr<CupsPrintJob> job) {
  SavePrintJob(job);
}

void PrintJobHistoryService::OnPrintJobError(base::WeakPtr<CupsPrintJob> job) {
  SavePrintJob(job);
}

void PrintJobHistoryService::OnPrintJobCancelled(
    base::WeakPtr<CupsPrintJob> job) {
  SavePrintJob(job);
}

void PrintJobHistoryService::SavePrintJob(base::WeakPtr<CupsPrintJob> job) {
  if (!job)
    return;
  print_job_database_->SavePrintJob(
      CupsPrintJobToProto(*job, /*id=*/base::GenerateGUID(), base::Time::Now()),
      base::BindOnce(&PrintJobHistoryService::OnPrintJobSaved,
                     base::Unretained(this)));
}

void PrintJobHistoryService::OnPrintJobSaved(bool success) {
  base::UmaHistogramBoolean("Printing.CUPS.PrintJobDatabasePrintJobSaved",
                            success);
  if (g_test_on_print_job_saved_callback)
    g_test_on_print_job_saved_callback->Run(success);
}

}  // namespace chromeos
