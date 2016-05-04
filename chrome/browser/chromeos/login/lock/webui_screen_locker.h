// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/shell_delegate.h"
#include "ash/wm/lock_state_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_delegate.h"
#include "chrome/browser/chromeos/login/signin_screen_controller.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/ui/lock_window.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/display_observer.h"
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
                          public keyboard::KeyboardControllerObserver,
                          public display::DisplayObserver,
                          public content::WebContentsObserver {
 public:
  explicit WebUIScreenLocker(ScreenLocker* screen_locker);

  // ScreenLockerDelegate:
  void LockScreen() override;
  void ScreenLockReady() override;
  void OnAuthenticate() override;
  void SetInputEnabled(bool enabled) override;
  void ShowErrorMessage(int error_msg_id,
                        HelpAppLauncher::HelpTopic help_topic_id) override;
  void ClearErrors() override;
  void AnimateAuthenticationSuccess() override;
  gfx::NativeWindow GetNativeWindow() const override;
  content::WebUI* GetAssociatedWebUI() override;
  void OnLockWebUIReady() override;
  void OnLockBackgroundDisplayed() override;
  void OnHeaderBarVisible() override;

  // LoginDisplay::Delegate:
  void CancelPasswordChangedFlow() override;
  void CompleteLogin(const UserContext& user_context) override;
  base::string16 GetConnectedNetworkName() override;
  bool IsSigninInProgress() const override;
  void Login(const UserContext& user_context,
             const SigninSpecifics& specifics) override;
  void MigrateUserData(const std::string& old_password) override;
  void OnSigninScreenReady() override;
  void OnStartEnterpriseEnrollment() override;
  void OnStartEnableDebuggingScreen() override;
  void OnStartKioskEnableScreen() override;
  void OnStartKioskAutolaunchScreen() override;
  void ShowWrongHWIDScreen() override;
  void ResetPublicSessionAutoLoginTimer() override;
  void ResyncUserData() override;
  void SetDisplayEmail(const std::string& email) override;
  void Signout() override;
  bool IsUserWhitelisted(const AccountId& account_id) override;

  // LockWindow::Observer:
  void OnLockWindowReady() override;

  // LockStateObserver:
  void OnLockStateEvent(ash::LockStateObserver::EventType event) override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // PowerManagerClient::Observer:
  void SuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;
  void LidEventReceived(bool open, const base::TimeTicks& time) override;

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  // TODO(jdufault): Remove PluginCrashed, PluginHungStatusChanged,
  // WebContentsDestroyed overrides once crbug.com/452599 is resolved.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void PluginHungStatusChanged(int plugin_child_id,
                               const base::FilePath& plugin_path,
                               bool is_hung) override;
  void WebContentsDestroyed() override;

  // ash::KeyboardStateObserver:
  void OnVirtualKeyboardStateChanged(bool activated) override;

  // keyboard::KeyboardControllerObserver:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI();

 private:
  friend class test::WebUIScreenLockerTester;

  ~WebUIScreenLocker() override;

  // Ensures that user pod is focused.
  void FocusUserPod();

  // Reset user pod and ensures that user pod is focused.
  void ResetAndFocusUserPod();

  // The screen locker window.
  views::Widget* lock_window_ = nullptr;

  // Sign-in Screen controller instance (owns login screens).
  std::unique_ptr<SignInScreenController> signin_screen_controller_;

  // Login UI implementation instance.
  std::unique_ptr<WebUILoginDisplay> login_display_;

  // Tracks when the lock window is displayed and ready.
  bool lock_ready_ = false;

  // Tracks when the WebUI finishes loading.
  bool webui_ready_ = false;

  // Time when lock was initiated, required for metrics.
  base::TimeTicks lock_time_;

  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  // True iff this object is observing a keyboard controller.
  bool is_observing_keyboard_ = false;

  base::WeakPtrFactory<WebUIScreenLocker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUIScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_
