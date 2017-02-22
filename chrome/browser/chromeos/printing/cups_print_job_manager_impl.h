// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/printers_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "printing/backend/cups_connection.h"

class Profile;

namespace chromeos {

class CupsPrintJobManagerImpl : public CupsPrintJobManager,
                                public content::NotificationObserver {
 public:
  explicit CupsPrintJobManagerImpl(Profile* profile);
  ~CupsPrintJobManagerImpl() override;

  // CupsPrintJobManager overrides:
  bool CancelPrintJob(CupsPrintJob* job) override;
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

  // Query CUPS for print job status.
  void QueryCups();

  // Process jobs from CUPS and perform notifications.
  void UpdateJobs(const std::vector<::printing::CupsJob>& jobs);

  // Updates the state and performs the appropriate notifications.
  void JobStateUpdated(CupsPrintJob* job, CupsPrintJob::State new_state);

  // Ongoing print jobs.
  std::map<std::string, std::unique_ptr<CupsPrintJob>> jobs_;

  // Prevents multiple queries from being scheduled simultaneously.
  bool in_query_ = false;

  ::printing::CupsConnection cups_connection_;
  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<CupsPrintJobManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_IMPL_H_
