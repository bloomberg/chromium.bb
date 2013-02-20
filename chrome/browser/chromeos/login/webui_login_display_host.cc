// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_display_host.h"

#include "ash/ash_switches.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "chrome://oobe/login";
// URL which corresponds to the OOBE WebUI.
const char kOobeURL[] = "chrome://oobe";

// Duration of sign-in transition animation.
const int kLoginFadeoutTransitionDurationMs = 700;

// Number of times we try to reload OOBE/login WebUI if it crashes.
const int kCrashCountLimit = 5;

// Whether to enable tnitializing WebUI in hidden state (see
// |initialize_webui_hidden_|) by default.
const bool kHiddenWebUIInitializationDefault = true;

// Switch values that might be used to override WebUI init type.
const char kWebUIInitParallel[] = "parallel";
const char kWebUIInitPostpone[] = "postpone";

}  // namespace

// WebUILoginDisplayHost -------------------------------------------------------

WebUILoginDisplayHost::WebUILoginDisplayHost(const gfx::Rect& background_bounds)
    : BaseLoginDisplayHost(background_bounds),
      login_window_(NULL),
      login_view_(NULL),
      webui_login_display_(NULL),
      is_showing_login_(false),
      is_wallpaper_loaded_(false),
      status_area_saved_visibility_(false),
      crash_count_(0),
      restore_path_(RESTORE_UNKNOWN),
      old_ignore_solo_window_frame_painter_policy_value_(false) {
  bool is_registered = WizardController::IsDeviceRegistered();
  bool zero_delay_enabled = WizardController::IsZeroDelayEnabled();
  bool disable_boot_animation = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableBootAnimation);
  bool disable_oobe_animation = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableOobeAnimation);
  bool new_oobe_ui = !CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewOobe);

  waiting_for_wallpaper_load_ =
      new_oobe_ui &&
      !zero_delay_enabled &&
      (is_registered || !disable_oobe_animation) &&
      (!is_registered || !disable_boot_animation);

  // For slower hardware we have boot animation disabled so
  // we'll be initializing WebUI hidden, waiting for user pods to load and then
  // show WebUI at once.
  waiting_for_user_pods_ =
      new_oobe_ui && !zero_delay_enabled && !waiting_for_wallpaper_load_;

  initialize_webui_hidden_ = kHiddenWebUIInitializationDefault &&
      new_oobe_ui && !zero_delay_enabled;

  is_boot_animation2_enabled_ = waiting_for_wallpaper_load_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshDisableBootAnimation2);

  // Prevents white flashing on OOBE (http://crbug.com/131569).
  aura::Env::GetInstance()->set_render_white_bg(false);

  // Check if WebUI init type is overriden.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshWebUIInit)) {
    const std::string override_type = CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kAshWebUIInit);
    if (override_type == kWebUIInitParallel)
      initialize_webui_hidden_ = true;
    else if (override_type == kWebUIInitPostpone)
      initialize_webui_hidden_ = false;
  }

  // Always postpone WebUI initialization on first boot, otherwise we miss
  // initial animation.
  if (!WizardController::IsOobeCompleted())
    initialize_webui_hidden_ = false;

  // There is no wallpaper for KioskMode, don't initialize the webui hidden.
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    initialize_webui_hidden_ = false;

  if (waiting_for_wallpaper_load_) {
    registrar_.Add(this, chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                   content::NotificationService::AllSources());
  }

  // In boot-animation2 we want to show login WebUI as soon as possible.
  if ((waiting_for_user_pods_ || is_boot_animation2_enabled_)
      && initialize_webui_hidden_) {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
  }
  LOG(INFO) << "Login WebUI >> "
            << "zero_delay: " << zero_delay_enabled
            << " wait_for_wp_load_: " << waiting_for_wallpaper_load_
            << " wait_for_pods_: " << waiting_for_user_pods_
            << " init_webui_hidden_: " << initialize_webui_hidden_;
}

WebUILoginDisplayHost::~WebUILoginDisplayHost() {
  ResetLoginWindowAndView();
}

// LoginDisplayHost implementation ---------------------------------------------

LoginDisplay* WebUILoginDisplayHost::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) {
  webui_login_display_ = new WebUILoginDisplay(delegate);
  webui_login_display_->set_background_bounds(background_bounds());
  return webui_login_display_;
}

gfx::NativeWindow WebUILoginDisplayHost::GetNativeWindow() const {
  return login_window_ ? login_window_->GetNativeWindow() : NULL;
}

views::Widget* WebUILoginDisplayHost::GetWidget() const {
  return login_window_;
}

void WebUILoginDisplayHost::OpenProxySettings() {
  if (login_view_)
    login_view_->OpenProxySettings();
}

void WebUILoginDisplayHost::SetOobeProgressBarVisible(bool visible) {
  GetOobeUI()->ShowOobeUI(visible);
}

void WebUILoginDisplayHost::SetShutdownButtonEnabled(bool enable) {
}

void WebUILoginDisplayHost::SetStatusAreaVisible(bool visible) {
  if (initialize_webui_hidden_)
    status_area_saved_visibility_ = visible;
  else if (login_view_)
    login_view_->SetStatusAreaVisible(visible);
}

void WebUILoginDisplayHost::StartWizard(const std::string& first_screen_name,
                                        DictionaryValue* screen_parameters) {
  // Keep parameters to restore if renderer crashes.
  restore_path_ = RESTORE_WIZARD;
  wizard_first_screen_name_ = first_screen_name;
  if (screen_parameters)
    wizard_screen_parameters_.reset(screen_parameters->DeepCopy());
  else
    wizard_screen_parameters_.reset(NULL);
  is_showing_login_ = false;
  scoped_ptr<DictionaryValue> scoped_parameters(screen_parameters);

  if (waiting_for_wallpaper_load_ && !initialize_webui_hidden_) {
    LOG(INFO) << "Login WebUI >> wizard postponed";
    return;
  }
  LOG(INFO) << "Login WebUI >> wizard";

  if (!login_window_)
    LoadURL(GURL(kOobeURL));

  BaseLoginDisplayHost::StartWizard(first_screen_name,
                                    scoped_parameters.release());
}

void WebUILoginDisplayHost::StartSignInScreen() {
  restore_path_ = RESTORE_SIGN_IN;
  is_showing_login_ = true;

  if (waiting_for_wallpaper_load_ && !initialize_webui_hidden_) {
    LOG(INFO) << "Login WebUI >> sign in postponed";
    return;
  }
  LOG(INFO) << "Login WebUI >> sign in";

  if (!login_window_)
    LoadURL(GURL(kLoginURL));

  BaseLoginDisplayHost::StartSignInScreen();
  CHECK(webui_login_display_);
  GetOobeUI()->ShowSigninScreen(webui_login_display_, webui_login_display_);
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    SetStatusAreaVisible(false);
}

void WebUILoginDisplayHost::OnPreferencesChanged() {
  if (is_showing_login_)
    webui_login_display_->OnPreferencesChanged();
}

void WebUILoginDisplayHost::OnBrowserCreated() {
  // Close lock window now so that the launched browser can receive focus.
  ResetLoginWindowAndView();
}

void WebUILoginDisplayHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED == type) {
    LOG(INFO) << "Login WebUI >> wp animation done";
    is_wallpaper_loaded_ = true;
    ash::Shell::GetInstance()->user_wallpaper_delegate()->
        OnWallpaperBootAnimationFinished();
    if (waiting_for_wallpaper_load_) {
      // StartWizard / StartSignInScreen could be called multiple times through
      // the lifetime of host.
      // Make sure that subsequent calls are not postponed.
      waiting_for_wallpaper_load_ = false;
      if (initialize_webui_hidden_)
        ShowWebUI();
      else
        StartPostponedWebUI();
    }
    registrar_.Remove(this,
                      chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                      content::NotificationService::AllSources());
  } else if (chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE == type) {
    LOG(INFO) << "Login WebUI >> WEBUI_VISIBLE";
    if (waiting_for_user_pods_ && initialize_webui_hidden_) {
      waiting_for_user_pods_ = false;
      ShowWebUI();
    } else if (waiting_for_wallpaper_load_ && initialize_webui_hidden_) {
      // Reduce time till login UI is shown - show it as soon as possible.
      waiting_for_wallpaper_load_ = false;
      ShowWebUI();
    }
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                      content::NotificationService::AllSources());
  } else {
    BaseLoginDisplayHost::Observe(type, source, details);
  }
}

void WebUILoginDisplayHost::LoadURL(const GURL& url) {
  InitLoginWindowAndView();
  // Subscribe to crash events.
  content::WebContentsObserver::Observe(login_view_->GetWebContents());
  login_view_->LoadURL(url);
}

void WebUILoginDisplayHost::RenderViewGone(base::TerminationStatus status) {
  // Do not try to restore on shutdown
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  crash_count_++;
  if (crash_count_ > kCrashCountLimit)
    return;

  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    // Restart if renderer has crashed.
    LOG(ERROR) << "Renderer crash on login window";
    switch (restore_path_) {
      case RESTORE_WIZARD:
        StartWizard(wizard_first_screen_name_,
                    wizard_screen_parameters_.release());
        break;
      case RESTORE_SIGN_IN:
        StartSignInScreen();
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

OobeUI* WebUILoginDisplayHost::GetOobeUI() const {
  if (!login_view_)
    return NULL;
  return static_cast<OobeUI*>(login_view_->GetWebUI()->GetController());
}

WizardController* WebUILoginDisplayHost::CreateWizardController() {
  // TODO(altimofeev): ensure that WebUI is ready.
  OobeDisplay* oobe_display = GetOobeUI();
  return new WizardController(this, oobe_display);
}

void WebUILoginDisplayHost::ShowWebUI() {
  if (!login_window_ || !login_view_) {
    NOTREACHED();
    return;
  }
  LOG(INFO) << "Login WebUI >> Show already initialized UI";
  login_window_->Show();
  login_view_->GetWebContents()->Focus();
  login_view_->SetStatusAreaVisible(status_area_saved_visibility_);
  login_view_->OnPostponedShow();
  // We should reset this flag to allow changing of status area visibility.
  initialize_webui_hidden_ = false;
}

void WebUILoginDisplayHost::StartPostponedWebUI() {
  if (!is_wallpaper_loaded_) {
    NOTREACHED();
    return;
  }
  LOG(INFO) << "Login WebUI >> Init postponed WebUI";

  // Wallpaper has finished loading before StartWizard/StartSignInScreen has
  // been called. In general this should not happen.
  // Let go through normal code path when one of those will be called.
  if (restore_path_ == RESTORE_UNKNOWN) {
    NOTREACHED();
    return;
  }

  switch (restore_path_) {
    case RESTORE_WIZARD:
      StartWizard(wizard_first_screen_name_,
                  wizard_screen_parameters_.release());
      break;
    case RESTORE_SIGN_IN:
      StartSignInScreen();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void WebUILoginDisplayHost::InitLoginWindowAndView() {
  if (login_window_)
    return;

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = background_bounds();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableNewOobe))
    params.transparent = true;
  params.parent =
      ash::Shell::GetContainer(
          ash::Shell::GetPrimaryRootWindow(),
          ash::internal::kShellWindowId_LockScreenContainer);

  login_window_ = new views::Widget;
  login_window_->Init(params);
  if (login_window_->GetNativeWindow()) {
    aura::RootWindow* root = login_window_->GetNativeWindow()->GetRootWindow();
    if (root) {
      old_ignore_solo_window_frame_painter_policy_value_ =
          root->GetProperty(ash::internal::kIgnoreSoloWindowFramePainterPolicy);
      root->SetProperty(ash::internal::kIgnoreSoloWindowFramePainterPolicy,
                        true);
    }
  }
  login_view_ = new WebUILoginView();

  login_view_->Init(login_window_);

  views::corewm::SetWindowVisibilityAnimationDuration(
      login_window_->GetNativeView(),
      base::TimeDelta::FromMilliseconds(kLoginFadeoutTransitionDurationMs));
  views::corewm::SetWindowVisibilityAnimationTransition(
      login_window_->GetNativeView(),
      views::corewm::ANIMATE_HIDE);

  login_window_->SetContentsView(login_view_);
  login_view_->UpdateWindowType();

  // If WebUI is initialized in hidden state, show it only if we're no
  // longer waiting for wallpaper animation/user images loading. Otherwise,
  // always show it.
  if (!initialize_webui_hidden_ ||
      (!waiting_for_wallpaper_load_ && !waiting_for_user_pods_)) {
    LOG(INFO) << "Login WebUI >> show login wnd on create";
    login_window_->Show();
  } else {
    LOG(INFO) << "Login WebUI >> login wnd is hidden on create";
    login_view_->set_is_hidden(true);
  }
  login_window_->GetNativeView()->SetName("WebUILoginView");
  login_view_->OnWindowCreated();
}

void WebUILoginDisplayHost::ResetLoginWindowAndView() {
  if (!login_window_)
    return;

  if (login_window_->GetNativeWindow()) {
    aura::RootWindow* root = login_window_->GetNativeWindow()->GetRootWindow();
    if (root) {
      root->SetProperty(ash::internal::kIgnoreSoloWindowFramePainterPolicy,
                        old_ignore_solo_window_frame_painter_policy_value_);
    }
  }
  login_window_->Close();
  login_window_ = NULL;
  login_view_ = NULL;
}

}  // namespace chromeos
