// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

#include <algorithm>

#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"

namespace chromeos {

CupsPrintJobManager::CupsPrintJobManager(Profile* profile) : profile_(profile) {
  notification_manager_.reset(
      new CupsPrintJobNotificationManager(profile, this));
}

CupsPrintJobManager::~CupsPrintJobManager() {
  print_jobs_.clear();
  notification_manager_.reset();
  profile_ = nullptr;
}

void CupsPrintJobManager::Shutdown() {}

void CupsPrintJobManager::AddObserver(Observer* observer) {
  auto found = std::find(observers_.begin(), observers_.end(), observer);
  if (found == observers_.end())
    observers_.push_back(observer);
}

void CupsPrintJobManager::RemoveObserver(Observer* observer) {
  auto found = std::find(observers_.begin(), observers_.end(), observer);
  if (found != observers_.end())
    observers_.erase(found);
}

}  // namespace chromeos
