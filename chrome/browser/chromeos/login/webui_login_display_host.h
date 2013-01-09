// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

class OobeUI;
class WebUILoginDisplay;
class WebUILoginView;

// WebUI-specific implementation of the OOBE/login screen host. Uses
// WebUILoginDisplay as the login screen UI implementation,
class WebUILoginDisplayHost : public BaseLoginDisplayHost,
                              public content::WebContentsObserver {
 public:
  explicit WebUILoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~WebUILoginDisplayHost();

  // LoginDisplayHost implementation:
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual views::Widget* GetWidget() const OVERRIDE;
  virtual void OpenProxySettings() OVERRIDE;
  virtual void SetOobeProgressBarVisible(bool visible) OVERRIDE;
  virtual void SetShutdownButtonEnabled(bool enable) OVERRIDE;
  virtual void SetStatusAreaVisible(bool visible) OVERRIDE;
  virtual void StartWizard(const std::string& first_screen_name,
                           DictionaryValue* screen_parameters) OVERRIDE;
  virtual void StartSignInScreen() OVERRIDE;
  virtual void OnPreferencesChanged() OVERRIDE;

  // BaseLoginDisplayHost overrides:
  virtual WizardController* CreateWizardController() OVERRIDE;
  virtual void OnBrowserCreated() OVERRIDE;

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI() const;

  // Returns the current login view.
  WebUILoginView* login_view() { return login_view_; }

 protected:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Overridden from content::WebContentsObserver:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  // Loads given URL. Creates WebUILoginView if needed.
  void LoadURL(const GURL& url);

  // Shows OOBE/sign in WebUI that was previously initialized in hidden state.
  void ShowWebUI();

  // Starts postponed WebUI (OOBE/sign in) if it was waiting for
  // wallpaper animation end.
  void StartPostponedWebUI();

  // Initializes |login_window_| and |login_view_| fields if needed.
  void InitLoginWindowAndView();

  // Closes |login_window_| and resets |login_window_| and
  // |login_view_| fields.
  void ResetLoginWindowAndView();

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

  content::NotificationRegistrar registrar_;

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

  DISALLOW_COPY_AND_ASSIGN(WebUILoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_
