// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/login/auth/auth_prewarmer.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/signin_screen_controller.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_removals_observer.h"
#include "ui/wm/public/scoped_drag_drop_disabler.h"

class AccountId;
class ScopedKeepAlive;

namespace ash {
class FocusRingController;
}

namespace chromeos {

class ArcKioskController;
class DemoAppLauncher;
class WebUILoginDisplay;
class WebUILoginView;

// An implementation class for OOBE/login WebUI screen host.
// It encapsulates controllers, wallpaper integration and flow.
class LoginDisplayHostImpl : public LoginDisplayHost,
                             public content::NotificationObserver,
                             public content::WebContentsObserver,
                             public chromeos::SessionManagerClient::Observer,
                             public chromeos::CrasAudioHandler::AudioObserver,
                             public display::DisplayObserver,
                             public ui::InputDeviceEventObserver,
                             public views::WidgetRemovalsObserver,
                             public chrome::MultiUserWindowManager::Observer {
 public:
  explicit LoginDisplayHostImpl(const gfx::Rect& wallpaper_bounds);
  ~LoginDisplayHostImpl() override;

  // LoginDisplayHost implementation:
  LoginDisplay* CreateLoginDisplay(LoginDisplay::Delegate* delegate) override;
  gfx::NativeWindow GetNativeWindow() const override;
  OobeUI* GetOobeUI() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void BeforeSessionStart() override;
  void Finalize(base::OnceClosure completion_callback) override;
  void OpenInternetDetailDialog(const std::string& network_id) override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(OobeScreen first_screen) override;
  WizardController* GetWizardController() override;
  AppLaunchController* GetAppLaunchController() override;
  void StartUserAdding(base::OnceClosure completion_callback) override;
  void CancelUserAdding() override;
  void StartSignInScreen(const LoginScreenContext& context) override;
  void OnPreferencesChanged() override;
  void PrewarmAuthentication() override;
  void StartAppLaunch(
      const std::string& app_id,
      bool diagnostic_mode,
      bool auto_launch) override;
  void StartDemoAppLaunch() override;
  void StartArcKiosk(const AccountId& account_id) override;
  bool IsVoiceInteractionOobe() override;
  void StartVoiceInteractionOobe() override;

  // Creates WizardController instance.
  WizardController* CreateWizardController();

  // Called when the first browser window is created, but before it's shown.
  void OnBrowserCreated();

  const gfx::Rect& wallpaper_bounds() const { return wallpaper_bounds_; }

  // Trace id for ShowLoginWebUI event (since there exists at most one login
  // WebUI at a time).
  static const int kShowLoginWebUIid;

  views::Widget* login_window_for_test() { return login_window_; }

  // Disable GaiaScreenHandler restrictive proxy check.
  static void DisableRestrictiveProxyCheckForTest();

 protected:
  class KeyboardDrivenOobeKeyHandler;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Overridden from content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;

  // Overridden from chromeos::SessionManagerClient::Observer:
  void EmitLoginPromptVisibleCalled() override;

  // Overridden from chromeos::CrasAudioHandler::AudioObserver:
  void OnActiveOutputNodeChanged() override;

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Overridden from ui::InputDeviceEventObserver
  void OnTouchscreenDeviceConfigurationChanged() override;

  // Overriden from views::WidgetRemovalsObserver:
  void OnWillRemoveView(views::Widget* widget, views::View* view) override;

  // Overriden from chrome::MultiUserWindowManager::Observer:
  void OnUserSwitchAnimationFinished() override;

 private:
  class LoginWidgetDelegate;

  // Way to restore if renderer have crashed.
  enum RestorePath {
    RESTORE_UNKNOWN,
    RESTORE_WIZARD,
    RESTORE_SIGN_IN,
    RESTORE_ADD_USER_INTO_SESSION,
  };

  // Type of animations to run after the login screen.
  enum FinalizeAnimationType {
    ANIMATION_NONE,       // No animation.
    ANIMATION_WORKSPACE,  // Use initial workspace animation (drop and
                          // and fade in workspace). Used for user login.
    ANIMATION_FADE_OUT,   // Fade out login screen. Used for app launch.
    ANIMATION_ADD_USER,   // Use UserSwitchAnimatorChromeOS animation when
                          // adding a user into multi-profile session.
  };

  // Marks display host for deletion.
  // If |post_quit_task| is true also posts Quit task to the MessageLoop.
  void ShutdownDisplayHost(bool post_quit_task);

  // Schedules workspace transition animation.
  void ScheduleWorkspaceAnimation();

  // Schedules fade out animation.
  void ScheduleFadeOutAnimation(int animation_speed_ms);

  // Loads given URL. Creates WebUILoginView if needed.
  void LoadURL(const GURL& url);

  // Shows OOBE/sign in WebUI that was previously initialized in hidden state.
  void ShowWebUI();

  // Starts postponed WebUI (OOBE/sign in) if it was waiting for
  // wallpaper animation end.
  void StartPostponedWebUI();

  // Initializes |login_window_| and |login_view_| fields if needed.
  void InitLoginWindowAndView();

  // Closes |login_window_| and resets |login_window_| and |login_view_| fields.
  void ResetLoginWindowAndView();

  // Deletes |auth_prewarmer_|.
  void OnAuthPrewarmDone();

  // Toggles OOBE progress bar visibility, the bar is hidden by default.
  void SetOobeProgressBarVisible(bool visible);

  // Tries to play startup sound. If sound can't be played right now,
  // for instance, because cras server is not initialized, playback
  // will be delayed.
  void TryToPlayStartupSound();

  // Called when login-prompt-visible signal is caught.
  void OnLoginPromptVisible();

  // Used to calculate position of the screens and wallpaper.
  gfx::Rect wallpaper_bounds_;

  content::NotificationRegistrar registrar_;

  // Sign in screen controller.
  std::unique_ptr<ExistingUserController> existing_user_controller_;

  // OOBE and some screens (camera, recovery) controller.
  std::unique_ptr<WizardController> wizard_controller_;

  std::unique_ptr<SignInScreenController> signin_screen_controller_;

  // App launch controller.
  std::unique_ptr<AppLaunchController> app_launch_controller_;

  // Demo app launcher.
  std::unique_ptr<DemoAppLauncher> demo_app_launcher_;

  // ARC kiosk controller.
  std::unique_ptr<ArcKioskController> arc_kiosk_controller_;

  // Make sure chrome won't exit while we are at login/oobe screen.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Has ShutdownDisplayHost() already been called?  Used to avoid posting our
  // own deletion to the message loop twice if the user logs out while we're
  // still in the process of cleaning up after login (http://crbug.com/134463).
  bool shutting_down_ = false;

  // Whether progress bar is shown on the OOBE page.
  bool oobe_progress_bar_visible_ = false;

  // True if session start is in progress.
  bool session_starting_ = false;

  // Container of the screen we are displaying.
  views::Widget* login_window_ = nullptr;

  // The delegate of |login_window_|; owned by |login_window_|.
  LoginWidgetDelegate* login_window_delegate_ = nullptr;

  // Container of the view we are displaying.
  WebUILoginView* login_view_ = nullptr;

  // Login display we are using.
  WebUILoginDisplay* webui_login_display_ = nullptr;

  // True if the login display is the current screen.
  bool is_showing_login_ = false;

  // True if NOTIFICATION_WALLPAPER_ANIMATION_FINISHED notification has been
  // received.
  bool is_wallpaper_loaded_ = false;

  // Stores status area current visibility to be applied once login WebUI
  // is shown.
  bool status_area_saved_visibility_ = false;

  // If true, WebUI is initialized in a hidden state and shown after the
  // wallpaper animation is finished (when it is enabled) or the user pods have
  // been loaded (otherwise).
  // By default is true. Could be used to tune performance if needed.
  bool initialize_webui_hidden_;

  // True if WebUI is initialized in hidden state and we're waiting for
  // wallpaper load animation to finish.
  bool waiting_for_wallpaper_load_;

  // True if WebUI is initialized in hidden state and we're waiting for user
  // pods to load.
  bool waiting_for_user_pods_;

  // How many times renderer has crashed.
  int crash_count_ = 0;

  // Way to restore if renderer have crashed.
  RestorePath restore_path_ = RESTORE_UNKNOWN;

  // Stored parameters for StartWizard, required to restore in case of crash.
  OobeScreen first_screen_;

  // Called after host deletion.
  std::vector<base::OnceClosure> completion_callbacks_;

  // Active instance of authentication prewarmer.
  std::unique_ptr<AuthPrewarmer> auth_prewarmer_;

  // A focus ring controller to draw focus ring around view for keyboard
  // driven oobe.
  std::unique_ptr<ash::FocusRingController> focus_ring_controller_;

  // Handles special keys for keyboard driven oobe.
  std::unique_ptr<KeyboardDrivenOobeKeyHandler>
      keyboard_driven_oobe_key_handler_;

  FinalizeAnimationType finalize_animation_type_ = ANIMATION_WORKSPACE;

  // Time when login prompt visible signal is received. Used for
  // calculations of delay before startup sound.
  base::TimeTicks login_prompt_visible_time_;

  // True when request to play startup sound was sent to
  // SoundsManager.
  // After OOBE is completed, this is always initialized with true.
  bool startup_sound_played_ = false;

  // Keeps a copy of the old Drag'n'Drop client, so that it would be disabled
  // during a login session and restored afterwards.
  std::unique_ptr<wm::ScopedDragDropDisabler> scoped_drag_drop_disabler_;

  bool is_voice_interaction_oobe_ = false;

  base::WeakPtrFactory<LoginDisplayHostImpl> pointer_factory_;
  base::WeakPtrFactory<LoginDisplayHostImpl> animation_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_IMPL_H_
