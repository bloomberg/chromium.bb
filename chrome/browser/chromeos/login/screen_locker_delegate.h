// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_DELEGATE_H_
#pragma once

#include "base/string16.h"

class GURL;

namespace chromeos {

class ScreenLocker;

// ScreenLockerDelegate takes care of displaying the lock screen UI.
class ScreenLockerDelegate {
 public:
  explicit ScreenLockerDelegate(ScreenLocker* screen_locker);
  virtual ~ScreenLockerDelegate();

  // Initialize the screen locker delegate. This will call ScreenLockReady when
  // done to notify ScreenLocker.
  virtual void LockScreen(bool unlock_on_input) = 0;

  // Inform the screen locker that the screen has been locked
  virtual void ScreenLockReady();

  // This function is called when ScreenLocker::Authenticate is called to
  // attempt to authenticate with a given password.
  virtual void OnAuthenticate() = 0;

  // Enable/disable password input.
  virtual void SetInputEnabled(bool enabled) = 0;

  // Enable/disable signout.
  virtual void SetSignoutEnabled(bool enabled) = 0;

  // Disables all UI needed and shows error bubble with |message|.
  // If |sign_out_only| is true then all other input except "Sign Out"
  // button is blocked.
  virtual void ShowErrorMessage(const string16& message,
                                bool sign_out_only) = 0;

  // Present user a CAPTCHA challenge with image from |captcha_url|,
  // After that shows error bubble with |message|.
  virtual void ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                          const string16& message) = 0;

  // Close message bubble to clear error messages.
  virtual void ClearErrors() = 0;

 protected:
  // ScreenLocker that owns this delegate.
  ScreenLocker* screen_locker_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_DELEGATE_H_
