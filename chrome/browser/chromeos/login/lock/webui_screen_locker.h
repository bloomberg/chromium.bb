// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_

#include <string>

#include "ash/shell_delegate.h"
#include "ash/wm/lock_state_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_delegate.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/ui/lock_window.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebUI;
}

namespace chromeos {

class OobeUI;
class ScreenLocker;
class WebUILoginDisplay;

namespace login {
class NetworkStateHelper;
}

namespace test {
class WebUIScreenLockerTester;
}

// This version of ScreenLockerDelegate displays a WebUI lock screen based on
// the Oobe account picker screen.
class WebUIScreenLocker : public WebUILoginView,
                          public LoginDisplay::Delegate,
                          public ScreenLockerDelegate,
                          public LockWindow::Observer,
                          public ash::LockStateObserver,
                          public views::WidgetObserver,
                          public PowerManagerClient::Observer,
                          public ash::VirtualKeyboardStateObserver,
                          public keyboard::KeyboardControllerObserver {
 public:
  explicit WebUIScreenLocker(ScreenLocker* screen_locker);

  // ScreenLockerDelegate implementation.
  virtual void LockScreen() OVERRIDE;
  virtual void ScreenLockReady() OVERRIDE;
  virtual void OnAuthenticate() OVERRIDE;
  virtual void SetInputEnabled(bool enabled) OVERRIDE;
  virtual void ShowErrorMessage(
      int error_msg_id,
      HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;
  virtual void ClearErrors() OVERRIDE;
  virtual void AnimateAuthenticationSuccess() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual content::WebUI* GetAssociatedWebUI() OVERRIDE;
  virtual void OnLockWebUIReady() OVERRIDE;
  virtual void OnLockBackgroundDisplayed() OVERRIDE;

  // LoginDisplay::Delegate: implementation
  virtual void CancelPasswordChangedFlow() OVERRIDE;
  virtual void CreateAccount() OVERRIDE;
  virtual void CompleteLogin(const UserContext& user_context) OVERRIDE;
  virtual base::string16 GetConnectedNetworkName() OVERRIDE;
  virtual bool IsSigninInProgress() const OVERRIDE;
  virtual void Login(const UserContext& user_context,
                     const SigninSpecifics& specifics) OVERRIDE;
  //  virtual void LoginAsRetailModeUser() OVERRIDE;
  //  virtual void LoginAsGuest() OVERRIDE;
  virtual void MigrateUserData(const std::string& old_password) OVERRIDE;
  //  virtual void LoginAsPublicAccount(const std::string& username) OVERRIDE;
  virtual void OnSigninScreenReady() OVERRIDE;
  virtual void OnStartEnterpriseEnrollment() OVERRIDE;
  virtual void OnStartKioskEnableScreen() OVERRIDE;
  virtual void OnStartKioskAutolaunchScreen() OVERRIDE;
  virtual void ShowWrongHWIDScreen() OVERRIDE;
  virtual void ResetPublicSessionAutoLoginTimer() OVERRIDE;
  virtual void ResyncUserData() OVERRIDE;
  virtual void SetDisplayEmail(const std::string& email) OVERRIDE;
  virtual void Signout() OVERRIDE;

  // content::NotificationObserver (via WebUILoginView) implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // LockWindow::Observer implementation.
  virtual void OnLockWindowReady() OVERRIDE;

  // LockStateObserver override.
  virtual void OnLockStateEvent(
      ash::LockStateObserver::EventType event) OVERRIDE;

  // WidgetObserver override.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // PowerManagerClient::Observer overrides:
  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE;
  virtual void LidEventReceived(bool open,
                                const base::TimeTicks& time) OVERRIDE;

  // Overridden from content::WebContentsObserver:
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;

  // Overridden from ash::KeyboardStateObserver:
  virtual void OnVirtualKeyboardStateChanged(bool activated) OVERRIDE;

  // Overridden from keyboard::KeyboardControllerObserver:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) OVERRIDE;

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI();

 private:
  friend class test::WebUIScreenLockerTester;

  virtual ~WebUIScreenLocker();

  // Ensures that user pod is focused.
  void FocusUserPod();

  // The screen locker window.
  views::Widget* lock_window_;

  // Login UI implementation instance.
  scoped_ptr<WebUILoginDisplay> login_display_;

  // Used for user image changed notifications.
  content::NotificationRegistrar registrar_;

  // Tracks when the lock window is displayed and ready.
  bool lock_ready_;

  // Tracks when the WebUI finishes loading.
  bool webui_ready_;

  // Time when lock was initiated, required for metrics.
  base::TimeTicks lock_time_;

  scoped_ptr<login::NetworkStateHelper> network_state_helper_;

  // True is subscribed as keyboard controller observer.
  bool is_observing_keyboard_;

  // The bounds of the virtual keyboard.
  gfx::Rect keyboard_bounds_;

  base::WeakPtrFactory<WebUIScreenLocker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_
