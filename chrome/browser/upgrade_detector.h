// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_H_

#include "base/timer.h"
#include "chrome/browser/idle.h"
#include "ui/gfx/image/image.h"

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
    UPGRADE_ANNOYANCE_CRITICAL,  // Red exclamation mark.
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

  // Whether the user should be notified about an upgrade.
  bool notify_upgrade() const { return notify_upgrade_; }

  // Whether the upgrade is a critical upgrade (such as a zero-day update).
  bool is_critical_update() const { return is_critical_upgrade_; }

  // Notifify this object that the user has acknowledged the critical update
  // so we don't need to complain about it for now.
  void acknowledge_critical_update() {
    critical_update_acknowledged_ = true;
  }

  // Whether the user has acknowledged the critical update.
  bool critical_update_acknowledged() const {
    return critical_update_acknowledged_;
  }

  // When the last upgrade was detected.
  const base::Time& upgrade_detected_time() const {
    return upgrade_detected_time_;
  }

  // Retrieves the right icon ID based on the degree of severity (see
  // UpgradeNotificationAnnoyanceLevel, each level has an an accompanying icon
  // to go with it). |type| determines which class of icons the caller wants,
  // either an icon appropriate for badging the wrench menu or one to display
  // within the wrench menu.
  int GetIconResourceID(UpgradeNotificationIconType type);

  UpgradeNotificationAnnoyanceLevel upgrade_notification_stage() const {
    return upgrade_notification_stage_;
  }

 protected:
  UpgradeDetector();

  // Sends out UPGRADE_DETECTED notification and record upgrade_detected_time_.
  void NotifyUpgradeDetected();

  // Sends out UPGRADE_RECOMMENDED notification and set notify_upgrade_.
  void NotifyUpgradeRecommended();

  void set_upgrade_notification_stage(UpgradeNotificationAnnoyanceLevel stage) {
    upgrade_notification_stage_ = stage;
  }

  // True if a critical update to Chrome has been installed, such as a zero-day
  // fix.
  bool is_critical_upgrade_;

  // Whether the user has acknowledged the critical update.
  bool critical_update_acknowledged_;

 private:
  // Initiates an Idle check. See IdleCallback below.
  void CheckIdle();

  // The callback for the IdleCheck. Tells us whether Chrome has received any
  // input events since the specified time.
  void IdleCallback(IdleState state);

  // When the upgrade was detected.
  base::Time upgrade_detected_time_;

  // A timer to check to see if we've been idle for long enough to show the
  // critical warning. Should only be set if |is_critical_upgrade_| is true.
  base::RepeatingTimer<UpgradeDetector> idle_check_timer_;

  // The stage at which the annoyance level for upgrade notifications is at.
  UpgradeNotificationAnnoyanceLevel upgrade_notification_stage_;

  // Whether we have waited long enough after detecting an upgrade (to see
  // is we should start nagging about upgrading).
  bool notify_upgrade_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetector);
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_H_
