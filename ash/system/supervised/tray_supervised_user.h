// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SUPERVISED_TRAY_SUPERVISED_USER_H_
#define ASH_SYSTEM_SUPERVISED_TRAY_SUPERVISED_USER_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace ash {

class SystemTray;

// System tray item that shows a message if the user is supervised or a child.
// Also shows a notification on login if the user is supervised. Shows a new
// notification if the user manager/custodian changes.
class ASH_EXPORT TraySupervisedUser : public SystemTrayItem,
                                      public SessionObserver {
 public:
  explicit TraySupervisedUser(SystemTray* system_tray);
  ~TraySupervisedUser() override;

  // SystemTrayItem:
  views::View* CreateDefaultView(LoginStatus status) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnUserSessionAdded(const AccountId& account_id) override;
  void OnUserSessionUpdated(const AccountId& account_id) override;

 private:
  friend class TraySupervisedUserTest;

  static const char kNotificationId[];

  void CreateOrUpdateNotification();

  base::string16 GetSupervisedUserMessage() const;

  std::string custodian_email_;
  std::string second_custodian_email_;

  ScopedSessionObserver scoped_session_observer_;

  DISALLOW_COPY_AND_ASSIGN(TraySupervisedUser);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SUPERVISED_TRAY_SUPERVISED_USER_H_
