// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector.h"
#include "components/variations/service/variations_service.h"

namespace base {
class SequencedTaskRunner;
class TaskRunner;
}

class UpgradeDetectorImpl : public UpgradeDetector,
                            public variations::VariationsService::Observer {
 public:
  ~UpgradeDetectorImpl() override;

  // Returns the currently installed Chrome version, which may be newer than the
  // one currently running. Not supported on Android, iOS or ChromeOS. Must be
  // run on a thread where I/O operations are allowed.
  static base::Version GetCurrentlyInstalledVersion();

  // Returns the global instance.
  static UpgradeDetectorImpl* GetInstance();

 protected:
  UpgradeDetectorImpl();

  // variations::VariationsService::Observer:
  void OnExperimentChangesDetected(Severity severity) override;

  // Trigger an "on upgrade" notification based on the specified |time_passed|
  // interval. Exposed as protected for testing.
  void NotifyOnUpgradeWithTimePassed(base::TimeDelta time_passed);

 private:
  // A callback that receives the results of |DetectUpgradeTask|.
  using UpgradeDetectedCallback = base::OnceCallback<void(UpgradeAvailable)>;

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  // Receives the results of AreAutoupdatesEnabled and starts the upgrade check
  // timer.
  void OnAutoupdatesEnabledResult(bool auto_updates_enabled);
#endif

  // Start the timer that will call |CheckForUpgrade()|.
  void StartTimerForUpgradeCheck();

  // Launches a background task to check if we have the latest version.
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

  // Determines whether or not an update is available, posting |callback| with
  // the result to |callback_task_runner| if so.
  static void DetectUpgradeTask(
      scoped_refptr<base::TaskRunner> callback_task_runner,
      UpgradeDetectedCallback callback);

  SEQUENCE_CHECKER(sequence_checker_);

  // A sequenced task runner on which blocking tasks run.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // We periodically check to see if Chrome has been upgraded.
  base::RepeatingTimer detect_upgrade_timer_;

  // After we detect an upgrade we start a recurring timer to see if enough time
  // has passed and we should start notifying the user.
  base::RepeatingTimer upgrade_notification_timer_;

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
