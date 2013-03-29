// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_SCREEN_LOCKER_H_

#include <string>

#include "ash/wm/session_state_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/lock_window.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/screen_locker_delegate.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebUI;
}

namespace chromeos {

class ScreenLocker;
class WebUILoginDisplay;
struct UserContext;

namespace test {
class WebUIScreenLockerTester;
}

// This version of ScreenLockerDelegate displays a WebUI lock screen based on
// the Oobe account picker screen.
class WebUIScreenLocker : public WebUILoginView,
                          public LoginDisplay::Delegate,
                          public ScreenLockerDelegate,
                          public LockWindow::Observer,
                          public ash::SessionStateObserver,
                          public views::WidgetObserver,
                          public PowerManagerClient::Observer {
 public:
  explicit WebUIScreenLocker(ScreenLocker* screen_locker);

  // ScreenLockerDelegate implementation.
  virtual void LockScreen(bool unlock_on_input) OVERRIDE;
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

  // LoginDisplay::Delegate: implementation
  virtual void CancelPasswordChangedFlow() OVERRIDE;
  virtual void CreateAccount() OVERRIDE;
  virtual void CompleteLogin(const UserContext& user_context) OVERRIDE;
  virtual string16 GetConnectedNetworkName() OVERRIDE;
  virtual void Login(const UserContext& user_context) OVERRIDE;
  virtual void LoginAsRetailModeUser() OVERRIDE;
  virtual void LoginAsGuest() OVERRIDE;
  virtual void MigrateUserData(const std::string& old_password) OVERRIDE;
  virtual void LoginAsPublicAccount(const std::string& username) OVERRIDE;
  virtual void OnSigninScreenReady() OVERRIDE;
  virtual void OnUserSelected(const std::string& username) OVERRIDE;
  virtual void OnStartEnterpriseEnrollment() OVERRIDE;
  virtual void OnStartDeviceReset() OVERRIDE;
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

  // SessionStateObserver override.
  virtual void OnSessionStateEvent(ash::SessionStateObserver::EventType event)
      OVERRIDE;

  // WidgetObserver override.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // PowerManagerClient::Observer overrides:
  virtual void SystemResumed(const base::TimeDelta& sleep_duration) OVERRIDE;
  virtual void LidEventReceived(bool open,
                                const base::TimeTicks& time) OVERRIDE;

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

  base::WeakPtrFactory<WebUIScreenLocker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_SCREEN_LOCKER_H_
