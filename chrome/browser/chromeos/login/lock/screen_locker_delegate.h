// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace content {
class WebUI;
}

namespace gfx {
class Image;
}

namespace chromeos {

class ScreenLocker;

// ScreenLockerDelegate takes care of displaying the lock screen UI.
class ScreenLockerDelegate {
 public:
  explicit ScreenLockerDelegate(ScreenLocker* screen_locker);
  virtual ~ScreenLockerDelegate();

  // Initialize the screen locker delegate. This will call ScreenLockReady when
  // done to notify ScreenLocker.
  virtual void LockScreen() = 0;

  // Inform the screen locker that the screen has been locked
  virtual void ScreenLockReady();

  // This function is called when ScreenLocker::Authenticate is called to
  // attempt to authenticate with a given password.
  virtual void OnAuthenticate() = 0;

  // Enable/disable password input.
  virtual void SetInputEnabled(bool enabled) = 0;

  // Disables all UI needed and shows error bubble with |message|.
  // If |sign_out_only| is true then all other input except "Sign Out"
  // button is blocked.
  virtual void ShowErrorMessage(
      int error_msg_id,
      HelpAppLauncher::HelpTopic help_topic_id) = 0;

  // Close message bubble to clear error messages.
  virtual void ClearErrors() = 0;

  // Allows to have visual effects once unlock authentication is successful,
  // Must call ScreenLocker::UnlockOnLoginSuccess() once all effects are done.
  virtual void AnimateAuthenticationSuccess() = 0;

  // Returns the native window displaying the lock screen.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns WebUI associated with screen locker implementation or NULL if
  // there isn't one.
  virtual content::WebUI* GetAssociatedWebUI();

  // Called when webui lock screen is ready.
  virtual void OnLockWebUIReady() = 0;

  // Called when webui lock screen wallpaper is loaded and displayed.
  virtual void OnLockBackgroundDisplayed() = 0;

  // Returns screen locker associated with delegate.
  ScreenLocker* screen_locker() { return screen_locker_; }

 protected:
  // ScreenLocker that owns this delegate.
  ScreenLocker* screen_locker_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_SCREEN_LOCKER_DELEGATE_H_
