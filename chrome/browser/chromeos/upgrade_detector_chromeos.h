// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UPGRADE_DETECTOR_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_UPGRADE_DETECTOR_CHROMEOS_H_
#pragma once

#include "base/timer.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/upgrade_detector.h"

template <typename T> struct DefaultSingletonTraits;

class UpgradeDetectorChromeos : public UpgradeDetector,
                                public chromeos::UpdateLibrary::Observer {
 public:
  virtual ~UpgradeDetectorChromeos();

  static UpgradeDetectorChromeos* GetInstance();

  // Initializes the object. Starts observing changes from the update
  // engine.
  void Init();

  // Shuts down the object. Stops observing observe changes from the
  // update engine.
  void Shutdown();

 private:
  friend struct DefaultSingletonTraits<UpgradeDetectorChromeos>;

  UpgradeDetectorChromeos();

  // chromeos::UpdateLibrary::Observer implementation.
  virtual void UpdateStatusChanged(
      const chromeos::UpdateLibrary::Status& status);

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that a new version is available.
  void NotifyOnUpgrade();

  // After we detect an upgrade we start a recurring timer to see if enough time
  // has passed and we should start notifying the user.
  base::RepeatingTimer<UpgradeDetectorChromeos> upgrade_notification_timer_;
  bool initialized_;
};

#endif  // CHROME_BROWSER_CHROMEOS_UPGRADE_DETECTOR_CHROMEOS_H_
