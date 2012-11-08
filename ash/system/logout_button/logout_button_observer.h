// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOGOUT_BUTTON_LOGOUT_BUTTON_OBSERVER_H_
#define ASH_SYSTEM_LOGOUT_BUTTON_LOGOUT_BUTTON_OBSERVER_H_

namespace ash {

// Observer for the value of the kShowLogoutButtonInTray pref that determines
// whether a logout button should be shown in the system tray during a session.
class LogoutButtonObserver {
 public:
  virtual ~LogoutButtonObserver() {}

  // Called when the value of the kShowLogoutButtonInTray pref changes.
  virtual void OnShowLogoutButtonInTrayChanged(bool show) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOGOUT_BUTTON_LOGOUT_BUTTON_OBSERVER_H_
