// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_WEBUI_SCREEN_LOCKER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/common/shell_observer.h"
#include "ash/common/wm/lock_state_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
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

class ScreenLocker;
class WebUILoginDisplay;

namespace login {
class NetworkStateHelper;
}

namespace test {
class WebUIScreenLockerTester;
}

// Displays a WebUI lock screen based on the Oobe account picker screen.
class WebUIScreenLocker : public WebUILoginView,
                          public LoginDisplay::Delegate,
                          public ash::LockStateObserver,
                          public views::WidgetObserver,
                          public PowerManagerClient::Observer,
                          public ash::ShellObserver,
                          public keyboard::KeyboardControllerObserver,
                          public display::DisplayObserver,
                          public content::WebContentsObserver {
 public:
  // Request lock screen preload when the user is idle. Does nothing if
  // preloading is disabled or if the preload hueristics return false.
  static void RequestPreload();

  explicit WebUIScreenLocker(ScreenLocker* screen_locker);
  ~WebUIScreenLocker() override;

  // Begin initializing the widget and views::WebView that show the lock screen.
  // ScreenLockReady is called when all initialization has finished.
  void LockScreen();

  // Enable/disable password input.
  void SetInputEnabled(bool enabled);

  // Disables all UI needed and shows error bubble with |message|. If
  // |sign_out_only| is true then all other input except "Sign Out" button is
  // blocked.
  void ShowErrorMessage(int error_msg_id,
                        HelpAppLauncher::HelpTopic help_topic_id);

  // Close message bubble to clear error messages.
  void ClearErrors();

  // Allows to have visual effects once unlock authentication is successful,
  // Must call ScreenLocker::UnlockOnLoginSuccess() once all effects are done.
  void AnimateAuthenticationSuccess();

  // Called when the webui lock screen is ready. This gets invoked by a
  // chrome.send from the embedded webui.
  void OnLockWebUIReady();

  // Called when webui lock screen wallpaper is loaded and displayed.
  void OnLockBackgroundDisplayed();

  // Called when the webui header bar becomes visible.
  void OnHeaderBarVisible();

 private:
  friend class test::WebUIScreenLockerTester;

  // Returns true if the lock screen should be preloaded.
  static bool ShouldPreloadLockScreen();
  // Helper function that creates and preloads a views::WebView.
  static std::unique_ptr<views::WebView> DoPreload(Profile* profile);

  // LoginDisplay::Delegate:
  void CancelPasswordChangedFlow() override;
  void CompleteLogin(const UserContext& user_context) override;
  base::string16 GetConnectedNetworkName() override;
  bool IsSigninInProgress() const override;
  void Login(const UserContext& user_context,
             const SigninSpecifics& specifics) override;
  void MigrateUserData(const std::string& old_password) override;
  void OnSigninScreenReady() override;
  void OnGaiaScreenReady() override;
  void OnStartEnterpriseEnrollment() override;
  void OnStartEnableDebuggingScreen() override;
  void OnStartKioskEnableScreen() override;
  void OnStartKioskAutolaunchScreen() override;
  void ShowWrongHWIDScreen() override;
  void ResetAutoLoginTimer() override;
  void ResyncUserData() override;
  void SetDisplayEmail(const std::string& email) override;
  void SetDisplayAndGivenName(const std::string& display_name,
                              const std::string& given_name) override;
  void Signout() override;
  bool IsUserWhitelisted(const AccountId& account_id) override;

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

  // ash::ShellObserver:
  void OnVirtualKeyboardStateChanged(bool activated) override;

  // keyboard::KeyboardControllerObserver:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;
  void OnKeyboardClosed() override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Inform the screen locker that the screen has been locked
  void ScreenLockReady();

  // Called when the lock window is ready.
  void OnLockWindowReady();

  // Returns the native window displaying the lock screen.
  gfx::NativeWindow GetNativeWindow() const;

  // Ensures that user pod is focused.
  void FocusUserPod();

  // Reset user pod and ensures that user pod is focused.
  void ResetAndFocusUserPod();

  // Configuration settings.
  WebViewSettings BuildConfigSettings();

  // The ScreenLocker that owns this instance.
  ScreenLocker* screen_locker_ = nullptr;

  // The screen locker window.
  LockWindow* lock_window_ = nullptr;

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
