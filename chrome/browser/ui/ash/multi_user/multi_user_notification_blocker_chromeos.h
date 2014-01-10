// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_NOTIFICATION_BLOCKER_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_NOTIFICATION_BLOCKER_CHROMEOS_H_

#include <map>
#include <set>
#include <string>

#include "ui/message_center/notification_blocker.h"

namespace chrome {
class MultiUserWindowManager;
}

// A notification blocker for per-profile stream switching. Owned and controlled
// by MultiUserWindowManagerChromeOS.
class MultiUserNotificationBlockerChromeOS
    : public message_center::NotificationBlocker {
 public:
  MultiUserNotificationBlockerChromeOS(
      message_center::MessageCenter* message_center,
      chrome::MultiUserWindowManager* multi_user_window_manager);
  virtual ~MultiUserNotificationBlockerChromeOS();

  // Checks the current desktop and update the list of users which owns windows
  // on the current desktop.
  void UpdateWindowOwners();

  // Called by MultiUserWindowManager when the active user has changed.
  void ActiveUserChanged(const std::string& user_id);

  // message_center::NotificationBlocker overrides:
  virtual bool ShouldShowNotification(
      const message_center::NotifierId& notifier_id) const OVERRIDE;
  virtual bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) const OVERRIDE;

 private:
  // Returns true if this blocker is actively working.
  bool IsActive() const;

  chrome::MultiUserWindowManager* multi_user_window_manager_;  // Weak.
  std::string active_user_id_;
  std::map<std::string, bool> quiet_modes_;
  std::set<std::string> current_user_ids_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserNotificationBlockerChromeOS);
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_NOTIFICATION_BLOCKER_CHROMEOS_H_
