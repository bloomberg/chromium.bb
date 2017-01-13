// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

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

    // Handle print job completion.  Note, |job| will be deleted after
    // notification is complete.
    virtual void OnPrintJobDone(CupsPrintJob* job) {}

    // Handle print job error.  Note, |job| will be deleted after
    // notification is complete.
    virtual void OnPrintJobError(CupsPrintJob* job) {}

    // Handle print job cancellation.  Note, |job| will be deleted after
    // notification is complete.
    virtual void OnPrintJobCancelled(CupsPrintJob* job) {}

   protected:
    virtual ~Observer() {}
  };

  static CupsPrintJobManager* CreateInstance(Profile* profile);

  explicit CupsPrintJobManager(Profile* profile);
  ~CupsPrintJobManager() override;

  // KeyedService override:
  void Shutdown() override;

  // Cancel a print job |job|. Note the |job| will be deleted after cancelled.
  virtual bool CancelPrintJob(CupsPrintJob* job) = 0;
  virtual bool SuspendPrintJob(CupsPrintJob* job) = 0;
  virtual bool ResumePrintJob(CupsPrintJob* job) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyJobCreated(CupsPrintJob* job);
  void NotifyJobStarted(CupsPrintJob* job);
  void NotifyJobUpdated(CupsPrintJob* job);
  void NotifyJobResumed(CupsPrintJob* job);
  void NotifyJobSuspended(CupsPrintJob* job);
  void NotifyJobCanceled(CupsPrintJob* job);
  void NotifyJobError(CupsPrintJob* job);
  void NotifyJobDone(CupsPrintJob* job);

  Profile* profile_;
  std::unique_ptr<CupsPrintJobNotificationManager> notification_manager_;
  base::ObserverList<Observer> observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_H_
