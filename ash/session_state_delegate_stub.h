// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_STATE_DELEGATE_STUB_H_
#define ASH_SESSION_STATE_DELEGATE_STUB_H_

#include "ash/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Stub implementation of SessionStateDelegate for testing.
class SessionStateDelegateStub : public SessionStateDelegate {
 public:
  SessionStateDelegateStub();
  virtual ~SessionStateDelegateStub();

  // SessionStateDelegate:
  virtual content::BrowserContext* GetBrowserContextByIndex(
      MultiProfileIndex index) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) OVERRIDE;
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
  virtual const base::string16 GetUserGivenName(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const std::string GetUserEmail(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const std::string GetUserID(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const gfx::ImageSkia& GetUserImage(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ShouldShowAvatar(aura::Window* window) OVERRIDE;
  virtual void SwitchActiveUser(const std::string& user_id) OVERRIDE;
  virtual void CycleActiveUser(CycleUser cycle_user) OVERRIDE;
  virtual void AddSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;
  virtual void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;

 private:
  bool screen_locked_;

  // A pseudo user image.
  gfx::ImageSkia user_image_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateStub);
};

}  // namespace ash

#endif  // ASH_SESSION_STATE_DELEGATE_STUB_H_
