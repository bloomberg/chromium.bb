// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_NOTIFICATION_BLOCKER_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_NOTIFICATION_BLOCKER_CHROMEOS_H_

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/message_center/notification_blocker.h"

// A notification blocker for per-profile stream switching. Owned and controlled
// by MultiUserWindowManagerChromeOS.
class MultiUserNotificationBlockerChromeOS
    : public message_center::NotificationBlocker {
 public:
  MultiUserNotificationBlockerChromeOS(
      message_center::MessageCenter* message_center,
      const AccountId& initial_account_id);
  ~MultiUserNotificationBlockerChromeOS() override;

  // Called by MultiUserWindowManager when the active user has changed.
  void ActiveUserChanged(const AccountId& account_id);

  // message_center::NotificationBlocker overrides:
  bool ShouldShowNotification(
      const message_center::Notification& notification) const override;
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

 private:
  // Returns true if this blocker is actively working.
  bool IsActive() const;

  AccountId active_account_id_;
  std::map<AccountId, bool> quiet_modes_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserNotificationBlockerChromeOS);
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_NOTIFICATION_BLOCKER_CHROMEOS_H_
