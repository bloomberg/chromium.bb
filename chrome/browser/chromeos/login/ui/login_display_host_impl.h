// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/login/auth/auth_prewarmer.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/signin_screen_controller.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

#if !defined(USE_ATHENA)
#include "ash/shell_delegate.h"
#endif

class PrefService;

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace chromeos {

class DemoAppLauncher;
class FocusRingController;
class OobeUI;
class WebUILoginDisplay;
class WebUILoginView;

#if !defined(USE_ATHENA)
class KeyboardDrivenOobeKeyHandler;
#endif

// An implementation class for OOBE/login WebUI screen host.
// It encapsulates controllers, background integration and flow.
class LoginDisplayHostImpl : public LoginDisplayHost,
                             public content::NotificationObserver,
                             public content::WebContentsObserver,
                             public chromeos::SessionManagerClient::Observer,
                             public chromeos::CrasAudioHandler::AudioObserver,
#if !defined(USE_ATHENA)
                             public ash::VirtualKeyboardStateObserver,
#endif
                             public keyboard::KeyboardControllerObserver,
                             public gfx::DisplayObserver,
                             public views::WidgetRemovalsObserver {
 public:
  explicit LoginDisplayHostImpl(const gfx::Rect& background_bounds);
  virtual ~LoginDisplayHostImpl();

  // Returns the default LoginDisplayHost instance if it has been created.
  static LoginDisplayHost* default_host() {
    return default_host_;
  }

  // LoginDisplayHost implementation:
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) override;
  virtual gfx::NativeWindow GetNativeWindow() const override;
  virtual WebUILoginView* GetWebUILoginView() const override;
  virtual void BeforeSessionStart() override;
  virtual void Finalize() override;
  virtual void OnCompleteLogin() override;
  virtual void OpenProxySettings() override;
  virtual void SetStatusAreaVisible(bool visible) override;
  virtual AutoEnrollmentController* GetAutoEnrollmentController() override;
  virtual void StartWizard(const std::string& first_screen_name) override;
  virtual WizardController* GetWizardController() override;
  virtual AppLaunchController* GetAppLaunchController() override;
  virtual void StartUserAdding(
      const base::Closure& completion_callback) override;
  virtual void StartSignInScreen(const LoginScreenContext& context) override;
  virtual void OnPreferencesChanged() override;
  virtual void PrewarmAuthentication() override;
  virtual void StartAppLaunch(const std::string& app_id,
                              bool diagnostic_mode) override;
  virtual void StartDemoAppLaunch() override;

  // Creates WizardController instance.
  WizardController* CreateWizardController();

  // Called when the first browser window is created, but before it's shown.
  void OnBrowserCreated();

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI() const;

  const gfx::Rect& background_bounds() const { return background_bounds_; }

  // Trace id for ShowLoginWebUI event (since there exists at most one login
  // WebUI at a time).
  static const int kShowLoginWebUIid;

  views::Widget* login_window_for_test() { return login_window_; }

 protected:
  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) override;

  // Overridden from content::WebContentsObserver:
  virtual void RenderProcessGone(base::TerminationStatus status) override;

  // Overridden from chromeos::SessionManagerClient::Observer:
  virtual void EmitLoginPromptVisibleCalled() override;

  // Overridden from chromeos::CrasAudioHandler::AudioObserver:
  virtual void OnActiveOutputNodeChanged() override;

#if !defined(USE_ATHENA)
  // Overridden from ash::KeyboardStateObserver:
  virtual void OnVirtualKeyboardStateChanged(bool activated) override;
#endif

  // Overridden from keyboard::KeyboardControllerObserver:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;

  // Overridden from gfx::DisplayObserver:
  virtual void OnDisplayAdded(const gfx::Display& new_display) override;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) override;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t changed_metrics) override;

  // Overriden from views::WidgetRemovalsObserver:
  virtual void OnWillRemoveView(views::Widget* widget,
                                views::View* view) override;

 private:
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
  };

  // Marks display host for deletion.
  // If |post_quit_task| is true also posts Quit task to the MessageLoop.
  void ShutdownDisplayHost(bool post_quit_task);

  // Schedules workspace transition animation.
  void ScheduleWorkspaceAnimation();

  // Schedules fade out animation.
  void ScheduleFadeOutAnimation();

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

  // Used to calculate position of the screens and background.
  gfx::Rect background_bounds_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<LoginDisplayHostImpl> pointer_factory_;

  // Default LoginDisplayHost.
  static LoginDisplayHost* default_host_;

  // The controller driving the auto-enrollment check.
  scoped_ptr<AutoEnrollmentController> auto_enrollment_controller_;

  // Sign in screen controller.
  scoped_ptr<ExistingUserController> existing_user_controller_;

  // OOBE and some screens (camera, recovery) controller.
  scoped_ptr<WizardController> wizard_controller_;

  scoped_ptr<SignInScreenController> signin_screen_controller_;

  // App launch controller.
  scoped_ptr<AppLaunchController> app_launch_controller_;

  // Demo app launcher.
  scoped_ptr<DemoAppLauncher> demo_app_launcher_;

  // Has ShutdownDisplayHost() already been called?  Used to avoid posting our
  // own deletion to the message loop twice if the user logs out while we're
  // still in the process of cleaning up after login (http://crbug.com/134463).
  bool shutting_down_;

  // Whether progress bar is shown on the OOBE page.
  bool oobe_progress_bar_visible_;

  // True if session start is in progress.
  bool session_starting_;

  // Container of the screen we are displaying.
  views::Widget* login_window_;

  // Container of the view we are displaying.
  WebUILoginView* login_view_;

  // Login display we are using.
  WebUILoginDisplay* webui_login_display_;

  // True if the login display is the current screen.
  bool is_showing_login_;

  // True if NOTIFICATION_WALLPAPER_ANIMATION_FINISHED notification has been
  // received.
  bool is_wallpaper_loaded_;

  // Stores status area current visibility to be applied once login WebUI
  // is shown.
  bool status_area_saved_visibility_;

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
  int crash_count_;

  // Way to restore if renderer have crashed.
  RestorePath restore_path_;

  // Stored parameters for StartWizard, required to restore in case of crash.
  std::string first_screen_name_;

  // Called before host deletion.
  base::Closure completion_callback_;

  // Active instance of authentication prewarmer.
  scoped_ptr<AuthPrewarmer> auth_prewarmer_;

  // A focus ring controller to draw focus ring around view for keyboard
  // driven oobe.
  scoped_ptr<FocusRingController> focus_ring_controller_;

#if !defined(USE_ATHENA)
  // Handles special keys for keyboard driven oobe.
  scoped_ptr<KeyboardDrivenOobeKeyHandler> keyboard_driven_oobe_key_handler_;
#endif

  FinalizeAnimationType finalize_animation_type_;

  // Time when login prompt visible signal is received. Used for
  // calculations of delay before startup sound.
  base::TimeTicks login_prompt_visible_time_;

  // True when request to play startup sound was sent to
  // SoundsManager.
  bool startup_sound_played_;

  // When true, startup sound should be played only when spoken
  // feedback is enabled.  Otherwise, startup sound should be played
  // in any case.
  bool startup_sound_honors_spoken_feedback_;

  // True is subscribed as keyboard controller observer.
  bool is_observing_keyboard_;

  // The bounds of the virtual keyboard.
  gfx::Rect keyboard_bounds_;

#if defined(USE_ATHENA)
  scoped_ptr<aura::Window> login_screen_container_;
#endif

  base::WeakPtrFactory<LoginDisplayHostImpl> animation_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_IMPL_H_
