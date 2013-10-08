// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_

#include "ash/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"

namespace ash {
class SessionStateObserver;
}  // namespace ash

class SessionStateDelegate : public ash::SessionStateDelegate {
 public:
  SessionStateDelegate();
  virtual ~SessionStateDelegate();

  // ash::SessionStateDelegate:
  virtual int GetMaximumNumberOfLoggedInUsers() const OVERRIDE;
  virtual int NumberOfLoggedInUsers() const OVERRIDE;
  virtual bool IsActiveUserSessionStarted() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual bool ShouldLockScreenBeforeSuspending() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;
  virtual bool IsUserSessionBlocked() const OVERRIDE;
  virtual const base::string16 GetUserDisplayName(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const std::string GetUserEmail(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const gfx::ImageSkia& GetUserImage(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual void GetLoggedInUsers(ash::UserIdList* users) OVERRIDE;
  virtual void SwitchActiveUser(const std::string& user_email) OVERRIDE;
  virtual void SwitchActiveUserToNext() OVERRIDE;
  virtual void AddSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;
  virtual void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_
