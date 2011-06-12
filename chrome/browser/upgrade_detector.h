// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_H_
#pragma once

#include "base/timer.h"
#include "ui/gfx/image.h"

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

  // The two types of icons we know about.
  enum UpgradeNotificationIconType {
    UPGRADE_ICON_TYPE_BADGE = 0,  // For overlay badging of the wrench menu.
    UPGRADE_ICON_TYPE_MENU_ICON,  // For showing in the wrench menu.
  };

  // Returns the singleton implementation instance.
  static UpgradeDetector* GetInstance();

  virtual ~UpgradeDetector();

  static void RegisterPrefs(PrefService* prefs);

  bool notify_upgrade() { return notify_upgrade_; }

  // Retrieves the right icon ID based on the degree of severity (see
  // UpgradeNotificationAnnoyanceLevel, each level has an an accompanying icon
  // to go with it). |type| determines which class of icons the caller wants,
  // either an icon appropriate for badging the wrench menu or one to display
  // within the wrench menu.
  int GetIconResourceID(UpgradeNotificationIconType type);

 protected:
  UpgradeDetector();

  // Sends out UPGRADE_DETECTED notification and record upgrade_detected_time_.
  void NotifyUpgradeDetected();

  // Sends out UPGRADE_RECOMMENDED notification and set notify_upgrade_.
  void NotifyUpgradeRecommended();

  // Accessors.
  const base::Time& upgrade_detected_time() const {
    return upgrade_detected_time_;
  }

  void set_upgrade_notification_stage(UpgradeNotificationAnnoyanceLevel stage) {
    upgrade_notification_stage_ = stage;
  }

 private:
  // When the upgrade was detected.
  base::Time upgrade_detected_time_;

  // The stage at which the annoyance level for upgrade notifications is at.
  UpgradeNotificationAnnoyanceLevel upgrade_notification_stage_;

  // Whether we have waited long enough after detecting an upgrade (to see
  // is we should start nagging about upgrading).
  bool notify_upgrade_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetector);
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_H_
