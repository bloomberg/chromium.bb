// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "chrome/browser/upgrade_detector.h"

template <typename T> struct DefaultSingletonTraits;

class UpgradeDetectorImpl : public UpgradeDetector {
 public:
  virtual ~UpgradeDetectorImpl();

  // Returns the singleton instance.
  static UpgradeDetectorImpl* GetInstance();

 private:
  friend struct DefaultSingletonTraits<UpgradeDetectorImpl>;

  UpgradeDetectorImpl();

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
  base::RepeatingTimer<UpgradeDetectorImpl> detect_upgrade_timer_;

  // After we detect an upgrade we start a recurring timer to see if enough time
  // has passed and we should start notifying the user.
  base::RepeatingTimer<UpgradeDetectorImpl> upgrade_notification_timer_;

  // We use this factory to create callback tasks for UpgradeDetected. We pass
  // the task to the actual upgrade detection code, which is in
  // DetectUpgradeTask.
  base::WeakPtrFactory<UpgradeDetectorImpl> weak_factory_;

  // True if this build is a dev or canary channel build.
  bool is_unstable_channel_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorImpl);
};


#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_IMPL_H_
