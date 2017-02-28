// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"

#include "base/memory/ptr_util.h"
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
      base::MakeUnique<CupsPrintJobNotification>(this, job, profile_);
}

void CupsPrintJobNotificationManager::OnPrintJobStarted(CupsPrintJob* job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobUpdated(CupsPrintJob* job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobSuspended(CupsPrintJob* job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobResumed(CupsPrintJob* job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobDone(CupsPrintJob* job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobError(CupsPrintJob* job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobCancelled(CupsPrintJob* job) {
  UpdateNotification(job);
}

void CupsPrintJobNotificationManager::OnPrintJobNotificationRemoved(
    CupsPrintJobNotification* notification) {
  // |job| might be a nullptr at this moment, so we iterate through
  // |notification_map_| to find |notification|.
  auto it = notification_map_.begin();
  for (; it != notification_map_.end(); it++) {
    if (it->second.get() == notification)
      break;
  }

  if (it != notification_map_.end())
    notification_map_.erase(it);
}

void CupsPrintJobNotificationManager::UpdateNotification(CupsPrintJob* job) {
  DCHECK(base::ContainsKey(notification_map_, job));
  notification_map_[job]->OnPrintJobStatusUpdated();
}

}  // namespace chromeos
