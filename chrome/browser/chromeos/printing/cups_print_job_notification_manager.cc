// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"

#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

CupsPrintJobNotificationManager::CupsPrintJobNotificationManager(
    Profile* profile,
    CupsPrintJobManager* print_job_manager)
    : print_job_manager_(print_job_manager), profile_(profile) {
  DCHECK(print_job_manager_);
  print_job_manager_->AddObserver(this);
}

CupsPrintJobNotificationManager::~CupsPrintJobNotificationManager() {
  DCHECK(print_job_manager_);
  print_job_manager_->RemoveObserver(this);
}

void CupsPrintJobNotificationManager::OnPrintJobCreated(CupsPrintJob* job) {
  if (base::ContainsKey(notification_map_, job))
    return;
  notification_map_[job] =
      base::MakeUnique<CupsPrintJobNotification>(job, profile_);
}

void CupsPrintJobNotificationManager::OnPrintJobStarted(CupsPrintJob* job) {
  DCHECK(base::ContainsKey(notification_map_, job));
  notification_map_[job]->OnPrintJobStatusUpdated();
}

void CupsPrintJobNotificationManager::OnPrintJobUpdated(CupsPrintJob* job) {
  DCHECK(base::ContainsKey(notification_map_, job));
  notification_map_[job]->OnPrintJobStatusUpdated();
}

void CupsPrintJobNotificationManager::OnPrintJobSuspended(CupsPrintJob* job) {
  DCHECK(base::ContainsKey(notification_map_, job));
  notification_map_[job]->OnPrintJobStatusUpdated();
}

void CupsPrintJobNotificationManager::OnPrintJobResumed(CupsPrintJob* job) {
  DCHECK(base::ContainsKey(notification_map_, job));
  notification_map_[job]->OnPrintJobStatusUpdated();
}

void CupsPrintJobNotificationManager::OnPrintJobDone(CupsPrintJob* job) {
  DCHECK(base::ContainsKey(notification_map_, job));
  notification_map_[job]->OnPrintJobStatusUpdated();
}

void CupsPrintJobNotificationManager::OnPrintJobError(CupsPrintJob* job) {
  DCHECK(base::ContainsKey(notification_map_, job));
  notification_map_[job]->OnPrintJobStatusUpdated();
}

}  // namespace chromeos
