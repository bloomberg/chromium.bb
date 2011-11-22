// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_SCREEN_LOCKER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/screen_locker_delegate.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/widget/widget.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/views/widget/native_widget_gtk.h"
#endif

namespace chromeos {

class ScreenLocker;
class WebUILoginDisplay;

// This version of ScreenLockerDelegate displays a WebUI lock screen based on
// the Oobe account picker screen.
class WebUIScreenLocker : public WebUILoginView,
                          public LoginDisplay::Delegate,
                          public content::NotificationObserver,
                          public ScreenLockerDelegate {
 public:
  explicit WebUIScreenLocker(ScreenLocker* screen_locker);

  // Called when the GTK grab breaks.
  void HandleGtkGrabBroke();

  // ScreenLockerDelegate implementation:
  virtual void LockScreen(bool unlock_on_input) OVERRIDE;
  virtual void ScreenLockReady() OVERRIDE;
  virtual void OnAuthenticate() OVERRIDE;
  virtual void SetInputEnabled(bool enabled) OVERRIDE;
  virtual void SetSignoutEnabled(bool enabled) OVERRIDE;
  virtual void ShowErrorMessage(const string16& message,
                                bool sign_out_only) OVERRIDE;
  virtual void ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                          const string16& message) OVERRIDE;
  virtual void ClearErrors() OVERRIDE;

  // LoginDisplay::Delegate: implementation
  virtual void CreateAccount() OVERRIDE;
  virtual string16 GetConnectedNetworkName() OVERRIDE;
  virtual void FixCaptivePortal() OVERRIDE;
  virtual void CompleteLogin(const std::string& username,
                             const std::string& password) OVERRIDE;
  virtual void Login(const std::string& username,
                     const std::string& password) OVERRIDE;
  virtual void LoginAsGuest() OVERRIDE;
  virtual void OnUserSelected(const std::string& username) OVERRIDE;
  virtual void OnStartEnterpriseEnrollment() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual ~WebUIScreenLocker();

  // Called when the window manager is ready to handle locked state.
  void OnWindowManagerReady();

  // Called when the all inputs are grabbed.
  void OnGrabInputs();

  // Clear current GTK grab.
  void ClearGtkGrab();

  // Try to grab all inputs. It initiates another try if it fails to
  // grab and the retry count is within a limit, or fails with CHECK.
  void TryGrabAllInputs();

  // This method tries to steal pointer/keyboard grab from other
  // client by sending events that will hopefully close menus or windows
  // that have the grab.
  void TryUngrabOtherClients();

  // Event handler for client-event.
  CHROMEGTK_CALLBACK_1(WebUIScreenLocker, void, OnClientEvent, GdkEventClient*)

  // The screen locker window.
  views::Widget* lock_window_;

  // Login UI implementation instance.
  scoped_ptr<WebUILoginDisplay> login_display_;

  // Used for user image changed notifications.
  content::NotificationRegistrar registrar_;

  // True if the screen locker's window has been drawn.
  bool drawn_;

  // True if both mouse input and keyboard input are grabbed.
  bool input_grabbed_;

  base::WeakPtrFactory<WebUIScreenLocker> weak_factory_;

  // The number times the widget tried to grab all focus.
  int grab_failure_count_;

  // Status of keyboard and mouse grab.
  GdkGrabStatus kbd_grab_status_;
  GdkGrabStatus mouse_grab_status_;

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_SCREEN_LOCKER_H_
