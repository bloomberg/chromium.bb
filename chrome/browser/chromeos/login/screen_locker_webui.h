// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_WEBUI_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_WEBUI_H_
#pragma once

#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/screen_locker_delegate.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"

#if defined(TOOLKIT_USES_GTK)
#include "views/widget/native_widget_gtk.h"
#endif

namespace views {
class ImageView;
}

namespace chromeos {

class InputEventObserver;
class ScreenLocker;
class ScreenLockWebUI;
class UserView;

namespace test {
class ScreenLockerTester;
}

class ScreenLockerWebUI : public ScreenLockerDelegate {
 public:
  explicit ScreenLockerWebUI(ScreenLocker* screen_locker);

  // ScreenLockerDelegate implementation:
  virtual void Init(bool unlock_on_input) OVERRIDE;
  virtual void ScreenLockReady() OVERRIDE;
  virtual void OnAuthenticate() OVERRIDE;
  virtual void SetInputEnabled(bool enabled) OVERRIDE;
  virtual void SetSignoutEnabled(bool enabled) OVERRIDE;
  virtual void ShowErrorMessage(const string16& message,
                                bool sign_out_only) OVERRIDE;
  virtual void ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                          const string16& message) OVERRIDE;
  virtual void ClearErrors() OVERRIDE;

 private:
  friend class ScreenLockWebUI;

  virtual ~ScreenLockerWebUI();

  // Called when the window manager is ready to handle locked state.
  void OnWindowManagerReady();

  // Event handler for client-event.
  CHROMEGTK_CALLBACK_1(ScreenLockerWebUI, void, OnClientEvent, GdkEventClient*);

  // The screen locker window.
  views::Widget* lock_window_;

  // A view that displays a WebUI lock screen.
  ScreenLockWebUI* screen_lock_webui_;

  // A message loop observer to detect user's keyboard/mouse event.
  // Used when |unlock_on_input_| is true.
  scoped_ptr<InputEventObserver> input_event_observer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerWebUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_WEBUI_H_
