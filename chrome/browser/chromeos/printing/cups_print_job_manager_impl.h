// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/printers_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "printing/backend/cups_connection.h"

class Profile;

namespace chromeos {

struct QueryResult {
  QueryResult();
  QueryResult(const QueryResult& other);
  ~QueryResult();

  bool success;
  std::vector<::printing::QueueStatus> queues;
};

// A wrapper around the CUPS connection to ensure that it's always accessed on
// the same sequence.
class CupsWrapper {
 public:
  CupsWrapper();
  ~CupsWrapper();

  // Query CUPS for the current jobs for the given |printer_ids|.  Writes result
  // to |result|.
  void QueryCups(const std::vector<std::string>& printer_ids,
                 QueryResult* result);

  // Cancel the print job on the blocking thread.
  void CancelJobImpl(const std::string& printer_id, const int job_id);

 private:
  ::printing::CupsConnection cups_connection_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CupsWrapper);
};

class CupsPrintJobManagerImpl : public CupsPrintJobManager,
                                public content::NotificationObserver {
 public:
  explicit CupsPrintJobManagerImpl(Profile* profile);
  ~CupsPrintJobManagerImpl() override;

  // CupsPrintJobManager overrides:
  void CancelPrintJob(CupsPrintJob* job) override;
  bool SuspendPrintJob(CupsPrintJob* job) override;
  bool ResumePrintJob(CupsPrintJob* job) override;

  // NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // Begin monitoring a print job for a given |printer_name| with the given
  // |title| with the pages |total_page_number|.
  bool CreatePrintJob(const std::string& printer_name,
                      const std::string& title,
                      int job_id,
                      int total_page_number);

  // Schedule a query of CUPS for print job status with the default delay.
  void ScheduleQuery();
  // Schedule a query of CUPS for print job status with a delay of |delay|.
  void ScheduleQuery(const base::TimeDelta& delay);

  // Schedule the CUPS query off the UI thread. Posts results back to UI thread
  // to UpdateJobs.
  void PostQuery();

  // Process jobs from CUPS and perform notifications.
  void UpdateJobs(std::unique_ptr<QueryResult> results);

  // Mark remaining jobs as errors and remove active jobs.
  void PurgeJobs();

  // Notify observers that a state update has occured for |job|.
  void NotifyJobStateUpdate(CupsPrintJob* job);

  // Ongoing print jobs.
  std::map<std::string, std::unique_ptr<CupsPrintJob>> jobs_;

  // Prevents multiple queries from being scheduled simultaneously.
  bool in_query_ = false;

  // Records the number of consecutive times the GetJobs query has failed.
  int retry_count_ = 0;

  content::NotificationRegistrar registrar_;
  // Task runner for queries to CUPS.
  scoped_refptr<base::SequencedTaskRunner> query_runner_;
  std::unique_ptr<CupsWrapper, base::OnTaskRunnerDeleter> cups_wrapper_;
  base::WeakPtrFactory<CupsPrintJobManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_IMPL_H_
