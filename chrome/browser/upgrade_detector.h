// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_H_
#pragma once

#include "base/timer.h"

template <typename T> struct DefaultSingletonTraits;
class PrefService;

///////////////////////////////////////////////////////////////////////////////
// UpgradeDetector
//
// This class is a singleton class that monitors when an upgrade happens in the
// background. We basically ask Omaha what it thinks the latest version is and
// if our version is lower we send out a notification upon:
//   a) Detecting an upgrade and...
//   b) When we think the user should be notified about the upgrade.
// The latter happens much later, since we don't want to be too annoying.
//
class UpgradeDetector {
 public:
  // The Homeland Security Upgrade Advisory System.
  enum UpgradeNotificationAnnoyanceLevel {
    UPGRADE_ANNOYANCE_NONE = 0,  // What? Me worry?
    UPGRADE_ANNOYANCE_LOW,       // Green.
    UPGRADE_ANNOYANCE_ELEVATED,  // Yellow.
    UPGRADE_ANNOYANCE_HIGH,      // Red.
    UPGRADE_ANNOYANCE_SEVERE,    // Orange.
  };

  // Returns the singleton instance.
  static UpgradeDetector* GetInstance();

  ~UpgradeDetector();

  static void RegisterPrefs(PrefService* prefs);

  bool notify_upgrade() { return notify_upgrade_; }

  UpgradeNotificationAnnoyanceLevel upgrade_notification_stage() const {
    return upgrade_notification_stage_;
  }

 private:
  friend struct DefaultSingletonTraits<UpgradeDetector>;

  UpgradeDetector();

  // Launches a task on the file thread to check if we have the latest version.
  void CheckForUpgrade();

  // Sends out a notification and starts a one shot timer to wait until
  // notifying the user.
  void UpgradeDetected();

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that a new version is available.
  void NotifyOnUpgrade();

  // We periodically check to see if Chrome has been upgraded.
  base::RepeatingTimer<UpgradeDetector> detect_upgrade_timer_;

  // After we detect an upgrade we start a recurring timer to see if enough time
  // has passed and we should start notifying the user.
  base::RepeatingTimer<UpgradeDetector> upgrade_notification_timer_;

  // We use this factory to create callback tasks for UpgradeDetected. We pass
  // the task to the actual upgrade detection code, which is in
  // DetectUpgradeTask.
  ScopedRunnableMethodFactory<UpgradeDetector> method_factory_;

  // When the upgrade was detected.
  base::Time upgrade_detected_time_;

  // Whether this build is a dev channel build or not.
  bool is_dev_channel_;

  // The stage at which the annoyance level for upgrade notifications is at.
  UpgradeNotificationAnnoyanceLevel upgrade_notification_stage_;

  // Whether we have waited long enough after detecting an upgrade (to see
  // is we should start nagging about upgrading).
  bool notify_upgrade_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetector);
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_H_
