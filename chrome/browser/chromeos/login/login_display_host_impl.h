// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/rect.h"

class PrefService;

namespace policy {
class AutoEnrollmentClient;
}  // namespace policy

namespace chromeos {

class OobeUI;
class WebUILoginDisplay;
class WebUILoginView;

// An implementation class for OOBE/login WebUI screen host.
// It encapsulates controllers, background integration and flow.
class LoginDisplayHostImpl : public LoginDisplayHost,
                             public content::NotificationObserver,
                             public content::WebContentsObserver {
 public:
  explicit LoginDisplayHostImpl(const gfx::Rect& background_bounds);
  virtual ~LoginDisplayHostImpl();

  // Returns the default LoginDispalyHost instance if it has been created.
  static LoginDisplayHost* default_host() {
    return default_host_;
  }

  // LoginDisplayHost implementation:
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual WebUILoginView* GetWebUILoginView() const OVERRIDE;
  virtual views::Widget* GetWidget() const OVERRIDE;
  virtual void BeforeSessionStart() OVERRIDE;
  virtual void OnSessionStart() OVERRIDE;
  virtual void OnCompleteLogin() OVERRIDE;
  virtual void OpenProxySettings() OVERRIDE;
  virtual void SetOobeProgressBarVisible(bool visible) OVERRIDE;
  virtual void SetShutdownButtonEnabled(bool enable) OVERRIDE;
  virtual void SetStatusAreaVisible(bool visible) OVERRIDE;
  virtual void CheckForAutoEnrollment() OVERRIDE;
  virtual void StartWizard(
      const std::string& first_screen_name,
      scoped_ptr<DictionaryValue> screen_parameters) OVERRIDE;
  virtual WizardController* GetWizardController() OVERRIDE;
  virtual void StartSignInScreen() OVERRIDE;
  virtual void ResumeSignInScreen() OVERRIDE;
  virtual void OnPreferencesChanged() OVERRIDE;

  // Creates WizardController instance.
  WizardController* CreateWizardController();

  // Called when the first browser window is created, but before it's shown.
  void OnBrowserCreated();

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI() const;

  const gfx::Rect& background_bounds() const { return background_bounds_; }

 protected:
  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from content::WebContentsObserver:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

 private:
  // Marks display host for deletion.
  // If |post_quit_task| is true also posts Quit task to the MessageLoop.
  void ShutdownDisplayHost(bool post_quit_task);

  // Start sign in transition animation.
  void StartAnimation();

  // Callback for the ownership status check.
  void OnOwnershipStatusCheckDone(DeviceSettingsService::OwnershipStatus status,
                                  bool current_user_is_owner);

  // Callback for completion of the |auto_enrollment_client_|.
  void OnAutoEnrollmentClientDone();

  // Forces auto-enrollment on the appropriate controller.
  void ForceAutoEnrollment();

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

  // Used to calculate position of the screens and background.
  gfx::Rect background_bounds_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<LoginDisplayHostImpl> pointer_factory_;

  // Default LoginDisplayHost.
  static LoginDisplayHost* default_host_;

  // Sign in screen controller.
  scoped_ptr<ExistingUserController> sign_in_controller_;

  // OOBE and some screens (camera, recovery) controller.
  scoped_ptr<WizardController> wizard_controller_;

  // Client for enterprise auto-enrollment check.
  scoped_ptr<policy::AutoEnrollmentClient> auto_enrollment_client_;

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

  // True if alternate boot animation is enabled.
  bool is_boot_animation2_enabled_;

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
  enum {
    RESTORE_UNKNOWN,
    RESTORE_WIZARD,
    RESTORE_SIGN_IN
  } restore_path_;

  // Stored parameters for StartWizard, required to restore in case of crash.
  std::string wizard_first_screen_name_;
  scoped_ptr<DictionaryValue> wizard_screen_parameters_;

  // Old value of the ash::internal::kIgnoreSoloWindowFramePainterPolicy
  // property of the root window for |login_window_|.
  bool old_ignore_solo_window_frame_painter_policy_value_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_DISPLAY_HOST_IMPL_H_
