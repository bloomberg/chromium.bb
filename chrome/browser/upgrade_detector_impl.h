// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/version.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/upgrade_detector.h"

template <typename T> struct DefaultSingletonTraits;

class UpgradeDetectorImpl :
    public UpgradeDetector,
    public chrome_variations::VariationsService::Observer {
 public:
  virtual ~UpgradeDetectorImpl();

  // Returns the currently installed Chrome version, which may be newer than the
  // one currently running. Not supported on Android, iOS or ChromeOS. Must be
  // run on a thread where I/O operations are allowed (e.g. FILE thread).
  static base::Version GetCurrentlyInstalledVersion();

  // Returns the singleton instance.
  static UpgradeDetectorImpl* GetInstance();

 protected:
  UpgradeDetectorImpl();

  // chrome_variations::VariationsService::Observer:
  virtual void OnExperimentChangesDetected(Severity severity) OVERRIDE;

  // Trigger an "on upgrade" notification based on the specified |time_passed|
  // interval. Exposed as protected for testing.
  void NotifyOnUpgradeWithTimePassed(base::TimeDelta time_passed);

 private:
  friend struct DefaultSingletonTraits<UpgradeDetectorImpl>;

  // Start the timer that will call |CheckForUpgrade()|.
  void StartTimerForUpgradeCheck();

  // Launches a task on the file thread to check if we have the latest version.
  void CheckForUpgrade();

  // Starts the upgrade notification timer that will check periodically whether
  // enough time has elapsed to update the severity (which maps to visual
  // badging) of the notification.
  void StartUpgradeNotificationTimer();

  // Sends out a notification and starts a one shot timer to wait until
  // notifying the user.
  void UpgradeDetected(UpgradeAvailable upgrade_available);

  // Returns true after calling UpgradeDetected if current install is outdated.
  bool DetectOutdatedInstall();

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that a new version is available.
  void NotifyOnUpgrade();

  // Called on the FILE thread to detect an upgrade. Calls back UpgradeDetected
  // on the UI thread if so. Although it looks weird, this needs to be a static
  // method receiving a WeakPtr<> to this object so that we can interrupt
  // the UpgradeDetected callback before it runs. Having this method non-static
  // and using |this| directly wouldn't be thread safe. And keeping it as a
  // non-class function would prevent it from calling UpgradeDetected.
  static void DetectUpgradeTask(
      base::WeakPtr<UpgradeDetectorImpl> upgrade_detector);

  // We periodically check to see if Chrome has been upgraded.
  base::RepeatingTimer<UpgradeDetectorImpl> detect_upgrade_timer_;

  // After we detect an upgrade we start a recurring timer to see if enough time
  // has passed and we should start notifying the user.
  base::RepeatingTimer<UpgradeDetectorImpl> upgrade_notification_timer_;

  // True if this build is a dev or canary channel build.
  bool is_unstable_channel_;

  // True if auto update is turned on.
  bool is_auto_update_enabled_;

  // When the upgrade was detected - either a software update or a variations
  // update, whichever happened first.
  base::TimeTicks upgrade_detected_time_;

  // The date the binaries were built.
  base::Time build_date_;

  // We use this factory to create callback tasks for UpgradeDetected. We pass
  // the task to the actual upgrade detection code, which is in
  // DetectUpgradeTask.
  base::WeakPtrFactory<UpgradeDetectorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorImpl);
};


#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_
