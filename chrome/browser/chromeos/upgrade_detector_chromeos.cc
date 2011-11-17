// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/upgrade_detector_chromeos.h"

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace {

// How long to wait (each cycle) before checking which severity level we should
// be at. Once we reach the highest severity, the timer will stop.
const int kNotifyCycleTimeMs = 20 * 60 * 1000;  // 20 minutes.

}  // namespace

UpgradeDetectorChromeos::UpgradeDetectorChromeos() : initialized_(false) {
}

UpgradeDetectorChromeos::~UpgradeDetectorChromeos() {
}

void UpgradeDetectorChromeos::Init() {
  if (chromeos::CrosLibrary::Get())
    chromeos::CrosLibrary::Get()->GetUpdateLibrary()->AddObserver(this);
  initialized_ = true;
}

void UpgradeDetectorChromeos::Shutdown() {
  // Init() may not be called from tests (ex. BrowserMainTest).
  if (!initialized_)
    return;
  if (chromeos::CrosLibrary::Get())
    chromeos::CrosLibrary::Get()->GetUpdateLibrary()->RemoveObserver(this);
}

void UpgradeDetectorChromeos::UpdateStatusChanged(
    const chromeos::UpdateLibrary::Status& status) {
  if (status.status != chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT)
    return;

  NotifyUpgradeDetected();

  // ChromeOS shows upgrade arrow once the upgrade becomes available.
  NotifyOnUpgrade();

  // Setup timer to to move along the upgrade advisory system.
  upgrade_notification_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kNotifyCycleTimeMs),
      this, &UpgradeDetectorChromeos::NotifyOnUpgrade);
}

void UpgradeDetectorChromeos::NotifyOnUpgrade() {
  base::TimeDelta delta = base::Time::Now() - upgrade_detected_time();
  int64 time_passed = delta.InDays();

  const int kSevereThreshold = 7;
  const int kHighThreshold = 4;
  const int kElevatedThreshold = 2;
  const int kLowThreshold = 0;

  // These if statements must be sorted (highest interval first).
  if (time_passed >= kSevereThreshold) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_SEVERE);

    // We can't get any higher, baby.
    upgrade_notification_timer_.Stop();
  } else if (time_passed >= kHighThreshold) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_HIGH);
  } else if (time_passed >= kElevatedThreshold) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_ELEVATED);
  } else if (time_passed >= kLowThreshold) {
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_LOW);
  } else {
    return;  // Not ready to recommend upgrade.
  }

  NotifyUpgradeRecommended();
}

// static
UpgradeDetectorChromeos* UpgradeDetectorChromeos::GetInstance() {
  return Singleton<UpgradeDetectorChromeos>::get();
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorChromeos::GetInstance();
}
