// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_

#include <vector>

#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace chromeos {

class CupsPrintJobNotificationManager;

class CupsPrintJobManager : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnPrintJobCreated(CupsPrintJob* job) {}
    virtual void OnPrintJobStarted(CupsPrintJob* job) {}
    virtual void OnPrintJobUpdated(CupsPrintJob* job) {}
    virtual void OnPrintJobSuspended(CupsPrintJob* job) {}
    virtual void OnPrintJobResumed(CupsPrintJob* job) {}
    virtual void OnPrintJobDone(CupsPrintJob* job) {}
    virtual void OnPrintJobError(CupsPrintJob* job) {}
    virtual void OnPrintJobCancelled(int job_id) {}

   protected:
    virtual ~Observer() {}
  };

  using PrintJobs = std::vector<std::unique_ptr<CupsPrintJob>>;

  explicit CupsPrintJobManager(Profile* profile);
  ~CupsPrintJobManager() override;

  // KeyedService override:
  void Shutdown() override;

  // Returns the CupsPrintJobManager instance that associated with the current
  // profile.
  static CupsPrintJobManager* Get(content::BrowserContext* browser_context);

  virtual bool CreatePrintJob(const std::string& printer_name,
                              const std::string& title,
                              int total_page_number) = 0;
  // Cancel a print job |job|. Note the |job| will be deleted after cancelled.
  virtual bool CancelPrintJob(CupsPrintJob* job) = 0;
  virtual bool SuspendPrintJob(CupsPrintJob* job) = 0;
  virtual bool ResumePrintJob(CupsPrintJob* job) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  PrintJobs print_jobs_;
  Profile* profile_;
  std::unique_ptr<CupsPrintJobNotificationManager> notification_manager_;
  std::vector<Observer*> observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
