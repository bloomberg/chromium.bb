// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_display_host_impl.h"

#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/frame_painter.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/ui/focus_ring_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/settings/timezone_settings.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "chrome://oobe/login";

// URL which corresponds to the OOBE WebUI.
const char kOobeURL[] = "chrome://oobe/oobe";

// URL which corresponds to the user adding WebUI.
const char kUserAddingURL[] = "chrome://oobe/user-adding";

// URL which corresponds to the app launch splash WebUI.
const char kAppLaunchSplashURL[] = "chrome://oobe/app-launch-splash";

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

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
const int64 kPolicyServiceInitializationDelayMilliseconds = 100;

// Determines the hardware keyboard from the given locale code
// and the OEM layout information, and saves it to "Locale State".
// The information will be used in InputMethodUtil::GetHardwareInputMethodId().
void DetermineAndSaveHardwareKeyboard(const std::string& locale,
                                      const std::string& oem_layout) {
  std::string layout;
  if (!oem_layout.empty()) {
    // If the OEM layout information is provided, use it.
    layout = oem_layout;
  } else {
    chromeos::input_method::InputMethodManager* manager =
        chromeos::input_method::InputMethodManager::Get();
    // Otherwise, determine the hardware keyboard from the locale.
    std::vector<std::string> input_method_ids;
    if (manager->GetInputMethodUtil()->GetInputMethodIdsFromLanguageCode(
            locale,
            chromeos::input_method::kKeyboardLayoutsOnly,
            &input_method_ids)) {
      // The output list |input_method_ids| is sorted by popularity, hence
      // input_method_ids[0] now contains the most popular keyboard layout
      // for the given locale.
      layout = input_method_ids[0];
    }
  }

  if (!layout.empty()) {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetString(prefs::kHardwareKeyboardLayout, layout);
    // This asks the file thread to save the prefs (i.e. doesn't block).
    // The latest values of Local State reside in memory so we can safely
    // get the value of kHardwareKeyboardLayout even if the data is not
    // yet saved to disk.
    prefs->CommitPendingWrite();
  }
}

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

namespace chromeos {

// static
LoginDisplayHost* LoginDisplayHostImpl::default_host_ = NULL;

// static
const int LoginDisplayHostImpl::kShowLoginWebUIid = 0x1111;

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, public

LoginDisplayHostImpl::LoginDisplayHostImpl(const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      pointer_factory_(this),
      shutting_down_(false),
      oobe_progress_bar_visible_(false),
      session_starting_(false),
      login_window_(NULL),
      login_view_(NULL),
      webui_login_display_(NULL),
      is_showing_login_(false),
      is_wallpaper_loaded_(false),
      status_area_saved_visibility_(false),
      crash_count_(0),
      restore_path_(RESTORE_UNKNOWN) {
  // We need to listen to CLOSE_ALL_BROWSERS_REQUEST but not APP_TERMINATING
  // because/ APP_TERMINATING will never be fired as long as this keeps
  // ref-count. CLOSE_ALL_BROWSERS_REQUEST is safe here because there will be no
  // browser instance that will block the shutdown.
  registrar_.Add(this,
                 chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                 content::NotificationService::AllSources());

  // NOTIFICATION_BROWSER_OPENED is issued after browser is created, but
  // not shown yet. Lock window has to be closed at this point so that
  // a browser window exists and the window can acquire input focus.
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllSources());

  // Login screen is moved to lock screen container when user logs in.
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());

  DCHECK(default_host_ == NULL);
  default_host_ = this;

  // Make sure chrome won't exit while we are at login/oobe screen.
  chrome::StartKeepAlive();

  bool is_registered = StartupUtils::IsDeviceRegistered();
  bool zero_delay_enabled = WizardController::IsZeroDelayEnabled();
  bool disable_boot_animation = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableBootAnimation);
  bool disable_oobe_animation = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableOobeAnimation);

  waiting_for_wallpaper_load_ =
      !zero_delay_enabled &&
      (is_registered || !disable_oobe_animation) &&
      (!is_registered || !disable_boot_animation);

  // For slower hardware we have boot animation disabled so
  // we'll be initializing WebUI hidden, waiting for user pods to load and then
  // show WebUI at once.
  waiting_for_user_pods_ = !zero_delay_enabled && !waiting_for_wallpaper_load_;

  initialize_webui_hidden_ =
      kHiddenWebUIInitializationDefault && !zero_delay_enabled;

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
  if (!StartupUtils::IsOobeCompleted())
    initialize_webui_hidden_ = false;

  // There is no wallpaper for KioskMode, don't initialize the webui hidden.
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    initialize_webui_hidden_ = false;

  if (waiting_for_wallpaper_load_) {
    registrar_.Add(this, chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED,
                   content::NotificationService::AllSources());
  }

  // When we wait for WebUI to be initialized we wait for one of
  // these notifications.
  if ((waiting_for_user_pods_ || waiting_for_wallpaper_load_)
      && initialize_webui_hidden_) {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                   content::NotificationService::AllSources());
  }
  LOG(WARNING) << "Login WebUI >> "
               << "zero_delay: " << zero_delay_enabled
               << " wait_for_wp_load_: " << waiting_for_wallpaper_load_
               << " wait_for_pods_: " << waiting_for_user_pods_
               << " init_webui_hidden_: " << initialize_webui_hidden_;
}

LoginDisplayHostImpl::~LoginDisplayHostImpl() {
  views::FocusManager::set_arrow_key_traversal_enabled(false);
  ResetLoginWindowAndView();

  // Let chrome process exit after login/oobe screen if needed.
  chrome::EndKeepAlive();

  default_host_ = NULL;
  // TODO(dzhioev): find better place for starting tutorial.
  if (CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kEnableFirstRunUI)) {
    // FirstRunController manages its lifetime and destructs after tutorial
    // completion.
    (new FirstRunController())->Start();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, LoginDisplayHost implementation:

LoginDisplay* LoginDisplayHostImpl::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) {
  if (system::keyboard_settings::ForceKeyboardDrivenUINavigation()) {
    views::FocusManager::set_arrow_key_traversal_enabled(true);

    focus_ring_controller_.reset(new FocusRingController);
    focus_ring_controller_->SetVisible(true);
  }
  webui_login_display_ = new WebUILoginDisplay(delegate);
  webui_login_display_->set_background_bounds(background_bounds());
  return webui_login_display_;
}

gfx::NativeWindow LoginDisplayHostImpl::GetNativeWindow() const {
  return login_window_ ? login_window_->GetNativeWindow() : NULL;
}

WebUILoginView* LoginDisplayHostImpl::GetWebUILoginView() const {
  return login_view_;
}

void LoginDisplayHostImpl::BeforeSessionStart() {
  session_starting_ = true;
}

void LoginDisplayHostImpl::Finalize() {
  DVLOG(1) << "Session starting";
  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()->
        desktop_background_controller()->MoveDesktopToUnlockedContainer();
  }
  if (wizard_controller_.get())
    wizard_controller_->OnSessionStart();
  if (!IsRunningUserAdding()) {
    // Display host is deleted once animation is completed
    // since sign in screen widget has to stay alive.
    if (ash::Shell::HasInstance()) {
      StartAnimation();
    }
  }
  ShutdownDisplayHost(false);
}

void LoginDisplayHostImpl::OnCompleteLogin() {
  // Cancelling the |auto_enrollment_client_| now allows it to determine whether
  // its protocol finished before login was complete.
  if (auto_enrollment_client_.get())
    auto_enrollment_client_.release()->CancelAndDeleteSoon();
}

void LoginDisplayHostImpl::OpenProxySettings() {
  if (login_view_)
    login_view_->OpenProxySettings();
}

void LoginDisplayHostImpl::SetStatusAreaVisible(bool visible) {
  if (initialize_webui_hidden_)
    status_area_saved_visibility_ = visible;
  else if (login_view_)
    login_view_->SetStatusAreaVisible(visible);
}

void LoginDisplayHostImpl::CheckForAutoEnrollment() {
  // This method is called when the controller determines that the
  // auto-enrollment check can start. This happens either after the EULA is
  // accepted, or right after a reboot if the EULA has already been accepted.

  if (policy::AutoEnrollmentClient::IsDisabled()) {
    VLOG(1) << "CheckForAutoEnrollment: auto-enrollment disabled";
    return;
  }

  // Start by checking if the device has already been owned.
  pointer_factory_.InvalidateWeakPtrs();
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&LoginDisplayHostImpl::OnOwnershipStatusCheckDone,
                 pointer_factory_.GetWeakPtr()));
}

void LoginDisplayHostImpl::StartWizard(
    const std::string& first_screen_name,
    scoped_ptr<DictionaryValue> screen_parameters) {
  // Keep parameters to restore if renderer crashes.
  restore_path_ = RESTORE_WIZARD;
  wizard_first_screen_name_ = first_screen_name;
  if (screen_parameters.get())
    wizard_screen_parameters_.reset(screen_parameters->DeepCopy());
  else
    wizard_screen_parameters_.reset();
  is_showing_login_ = false;

  if (waiting_for_wallpaper_load_ && !initialize_webui_hidden_) {
    LOG(WARNING) << "Login WebUI >> wizard postponed";
    return;
  }
  LOG(WARNING) << "Login WebUI >> wizard";

  if (!login_window_)
    LoadURL(GURL(kOobeURL));

  DVLOG(1) << "Starting wizard, first_screen_name: " << first_screen_name;
  // Create and show the wizard.
  // Note, dtor of the old WizardController should be called before ctor of the
  // new one, because "default_controller()" is updated there. So pure "reset()"
  // is done before new controller creation.
  wizard_controller_.reset();
  wizard_controller_.reset(CreateWizardController());

  oobe_progress_bar_visible_ = !StartupUtils::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  wizard_controller_->Init(first_screen_name, screen_parameters.Pass());
}

WizardController* LoginDisplayHostImpl::GetWizardController() {
  return wizard_controller_.get();
}

AppLaunchController* LoginDisplayHostImpl::GetAppLaunchController() {
  return app_launch_controller_.get();
}

void LoginDisplayHostImpl::StartUserAdding(
    const base::Closure& completion_callback) {
  restore_path_ = RESTORE_ADD_USER_INTO_SESSION;
  completion_callback_ = completion_callback;
  LOG(WARNING) << "Login WebUI >> user adding";
  if (!login_window_)
    LoadURL(GURL(kUserAddingURL));
  // We should emit this signal only at login screen (after reboot or sign out).
  login_view_->set_should_emit_login_prompt_visible(false);

  // Lock container can be transparent after lock screen animation.
  aura::Window* lock_container = ash::Shell::GetContainer(
      ash::Shell::GetPrimaryRootWindow(),
      ash::internal::kShellWindowId_LockScreenContainersContainer);
  lock_container->layer()->SetOpacity(1.0);

  ash::Shell::GetInstance()->
      desktop_background_controller()->MoveDesktopToLockedContainer();

  sign_in_controller_.reset();  // Only one controller in a time.
  sign_in_controller_.reset(new chromeos::ExistingUserController(this));
  SetOobeProgressBarVisible(oobe_progress_bar_visible_ = false);
  SetStatusAreaVisible(true);
  sign_in_controller_->Init(
      chromeos::UserManager::Get()->GetUsersAdmittedForMultiProfile());
  CHECK(webui_login_display_);
  GetOobeUI()->ShowSigninScreen(webui_login_display_, webui_login_display_);
}

void LoginDisplayHostImpl::StartSignInScreen() {
  restore_path_ = RESTORE_SIGN_IN;
  is_showing_login_ = true;

  PrewarmAuthentication();

  if (waiting_for_wallpaper_load_ && !initialize_webui_hidden_) {
    LOG(WARNING) << "Login WebUI >> sign in postponed";
    return;
  }
  LOG(WARNING) << "Login WebUI >> sign in";

  if (!login_window_) {
    TRACE_EVENT_ASYNC_BEGIN0("ui", "ShowLoginWebUI", kShowLoginWebUIid);
    LoadURL(GURL(kLoginURL));
  }

  DVLOG(1) << "Starting sign in screen";
  const chromeos::UserList& users = chromeos::UserManager::Get()->GetUsers();

  // Fix for users who updated device and thus never passed register screen.
  // If we already have users, we assume that it is not a second part of
  // OOBE. See http://crosbug.com/6289
  if (!StartupUtils::IsDeviceRegistered() && !users.empty()) {
    VLOG(1) << "Mark device registered because there are remembered users: "
            << users.size();
    StartupUtils::MarkDeviceRegistered();
  }

  sign_in_controller_.reset();  // Only one controller in a time.
  sign_in_controller_.reset(new chromeos::ExistingUserController(this));
  oobe_progress_bar_visible_ = !StartupUtils::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  SetStatusAreaVisible(true);
  sign_in_controller_->Init(users);

  // We might be here after a reboot that was triggered after OOBE was complete,
  // so check for auto-enrollment again. This might catch a cached decision from
  // a previous oobe flow, or might start a new check with the server.
  CheckForAutoEnrollment();

  // Initiate services customization manifest fetching.
  ServicesCustomizationDocument::GetInstance()->StartFetching();

  // Initiate mobile config load.
  MobileConfig::GetInstance();

  // Initiate device policy fetching.
  g_browser_process->browser_policy_connector()->ScheduleServiceInitialization(
      kPolicyServiceInitializationDelayMilliseconds);

  CHECK(webui_login_display_);
  GetOobeUI()->ShowSigninScreen(webui_login_display_, webui_login_display_);
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    SetStatusAreaVisible(false);
}

void LoginDisplayHostImpl::ResumeSignInScreen() {
  // We only get here after a previous call the StartSignInScreen. That sign-in
  // was successful but was interrupted by an auto-enrollment execution; once
  // auto-enrollment is complete we resume the normal login flow from here.
  DVLOG(1) << "Resuming sign in screen";
  CHECK(sign_in_controller_.get());
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  SetStatusAreaVisible(true);
  sign_in_controller_->ResumeLogin();
}


void LoginDisplayHostImpl::OnPreferencesChanged() {
  if (is_showing_login_)
    webui_login_display_->OnPreferencesChanged();
}

void LoginDisplayHostImpl::PrewarmAuthentication() {
  auth_prewarmer_.reset(new AuthPrewarmer());
  auth_prewarmer_->PrewarmAuthentication(
      base::Bind(&LoginDisplayHostImpl::OnAuthPrewarmDone,
                 pointer_factory_.GetWeakPtr()));
}

void LoginDisplayHostImpl::StartAppLaunch(const std::string& app_id) {
  LOG(WARNING) << "Login WebUI >> start app launch.";
  SetStatusAreaVisible(false);
  if (!login_window_)
    LoadURL(GURL(kAppLaunchSplashURL));

  login_view_->set_should_emit_login_prompt_visible(false);

  app_launch_controller_.reset(new AppLaunchController(
      app_id, this, GetOobeUI()));

  app_launch_controller_->StartAppLaunch();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, public

WizardController* LoginDisplayHostImpl::CreateWizardController() {
  // TODO(altimofeev): ensure that WebUI is ready.
  OobeDisplay* oobe_display = GetOobeUI();
  return new WizardController(this, oobe_display);
}

void LoginDisplayHostImpl::OnBrowserCreated() {
  // Close lock window now so that the launched browser can receive focus.
  ResetLoginWindowAndView();
}

OobeUI* LoginDisplayHostImpl::GetOobeUI() const {
  if (!login_view_)
    return NULL;
  return static_cast<OobeUI*>(login_view_->GetWebUI()->GetController());
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, content:NotificationObserver implementation:

void LoginDisplayHostImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (chrome::NOTIFICATION_WALLPAPER_ANIMATION_FINISHED == type) {
    LOG(WARNING) << "Login WebUI >> wp animation done";
    is_wallpaper_loaded_ = true;
    ash::Shell::GetInstance()->user_wallpaper_delegate()
        ->OnWallpaperBootAnimationFinished();
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
  } else if (chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE == type ||
             chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN == type) {
    LOG(WARNING) << "Login WebUI >> WEBUI_VISIBLE";
    if (waiting_for_user_pods_ && initialize_webui_hidden_) {
      waiting_for_user_pods_ = false;
      ShowWebUI();
    } else if (waiting_for_wallpaper_load_ && initialize_webui_hidden_) {
      // Reduce time till login UI is shown - show it as soon as possible.
      waiting_for_wallpaper_load_ = false;
      ShowWebUI();
    }
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                      content::NotificationService::AllSources());
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                      content::NotificationService::AllSources());
  } else if (type == chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST) {
    ShutdownDisplayHost(true);
  } else if (type == chrome::NOTIFICATION_BROWSER_OPENED && session_starting_) {
    // Browsers created before session start (windows opened by extensions, for
    // example) are ignored.
    OnBrowserCreated();
    registrar_.Remove(this,
                      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                      content::NotificationService::AllSources());
    registrar_.Remove(this,
                      chrome::NOTIFICATION_BROWSER_OPENED,
                      content::NotificationService::AllSources());
  } else if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED &&
             chromeos::UserManager::Get()->IsCurrentUserNew()) {
    // For new user, move desktop to locker container so that windows created
    // during the user image picker step are below it.
    ash::Shell::GetInstance()->
        desktop_background_controller()->MoveDesktopToLockedContainer();
    registrar_.Remove(this,
                      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                      content::NotificationService::AllSources());
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, WebContentsObserver implementation:

void LoginDisplayHostImpl::RenderProcessGone(base::TerminationStatus status) {
  // Do not try to restore on shutdown
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  crash_count_++;
  if (crash_count_ > kCrashCountLimit)
    return;

  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    // Render with login screen crashed. Let's crash browser process to let
    // session manager restart it properly. It is hard to reload the page
    // and get to controlled state that is fully functional.
    // If you see check, search for renderer crash for the same client.
    LOG(FATAL) << "Renderer crash on login window";
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostImpl, private

void LoginDisplayHostImpl::ShutdownDisplayHost(bool post_quit_task) {
  if (shutting_down_)
    return;

  shutting_down_ = true;
  registrar_.RemoveAll();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  if (post_quit_task)
    base::MessageLoop::current()->Quit();

  if (!completion_callback_.is_null())
    completion_callback_.Run();
}

void LoginDisplayHostImpl::StartAnimation() {
  if (ash::Shell::GetContainer(
          ash::Shell::GetPrimaryRootWindow(),
          ash::internal::kShellWindowId_DesktopBackgroundContainer)->
          children().empty()) {
    // If there is no background window, don't perform any animation on the
    // default and background layer because there is nothing behind it.
    return;
  }

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLoginAnimations))
    ash::Shell::GetInstance()->DoInitialWorkspaceAnimation();
}

void LoginDisplayHostImpl::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  if (status != DeviceSettingsService::OWNERSHIP_NONE) {
    // The device is already owned. No need for auto-enrollment checks.
    VLOG(1) << "CheckForAutoEnrollment: device already owned";
    return;
  }

  // Kick off the auto-enrollment client.
  if (auto_enrollment_client_.get()) {
    // They client might have been started after the EULA screen, but we made
    // it to the login screen before it finished. In that case let the current
    // client proceed.
    //
    // CheckForAutoEnrollment() is also called when we reach the sign-in screen,
    // because that's what happens after an auto-update.
    VLOG(1) << "CheckForAutoEnrollment: client already started";

    // If the client already started and already finished too, pass the decision
    // to the |sign_in_controller_| now.
    if (auto_enrollment_client_->should_auto_enroll())
      ForceAutoEnrollment();
  } else {
    VLOG(1) << "CheckForAutoEnrollment: starting auto-enrollment client";
    auto_enrollment_client_.reset(policy::AutoEnrollmentClient::Create(
        base::Bind(&LoginDisplayHostImpl::OnAutoEnrollmentClientDone,
                   base::Unretained(this))));
    auto_enrollment_client_->Start();
  }
}

void LoginDisplayHostImpl::OnAutoEnrollmentClientDone() {
  bool auto_enroll = auto_enrollment_client_->should_auto_enroll();
  VLOG(1) << "OnAutoEnrollmentClientDone, decision is " << auto_enroll;

  if (auto_enroll)
    ForceAutoEnrollment();
}

void LoginDisplayHostImpl::ForceAutoEnrollment() {
  if (sign_in_controller_.get())
    sign_in_controller_->DoAutoEnrollment();
}

void LoginDisplayHostImpl::LoadURL(const GURL& url) {
  InitLoginWindowAndView();
  // Subscribe to crash events.
  content::WebContentsObserver::Observe(login_view_->GetWebContents());
  login_view_->LoadURL(url);
}

void LoginDisplayHostImpl::ShowWebUI() {
  if (!login_window_ || !login_view_) {
    NOTREACHED();
    return;
  }
  LOG(WARNING) << "Login WebUI >> Show already initialized UI";
  login_window_->Show();
  login_view_->GetWebContents()->GetView()->Focus();
  login_view_->SetStatusAreaVisible(status_area_saved_visibility_);
  login_view_->OnPostponedShow();
  // We should reset this flag to allow changing of status area visibility.
  initialize_webui_hidden_ = false;
}

void LoginDisplayHostImpl::StartPostponedWebUI() {
  if (!is_wallpaper_loaded_) {
    NOTREACHED();
    return;
  }
  LOG(WARNING) << "Login WebUI >> Init postponed WebUI";

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
                  wizard_screen_parameters_.Pass());
      break;
    case RESTORE_SIGN_IN:
      StartSignInScreen();
      break;
    case RESTORE_ADD_USER_INTO_SESSION:
      StartUserAdding(completion_callback_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LoginDisplayHostImpl::InitLoginWindowAndView() {
  if (login_window_)
    return;
  ash::FramePainter::SetSoloWindowHeadersEnabled(false);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = background_bounds();
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      ash::Shell::GetContainer(
          ash::Shell::GetPrimaryRootWindow(),
          ash::internal::kShellWindowId_LockScreenContainer);

  login_window_ = new views::Widget;
  login_window_->Init(params);
  login_view_ = new WebUILoginView();
  login_view_->Init();

  views::corewm::SetWindowVisibilityAnimationDuration(
      login_window_->GetNativeView(),
      base::TimeDelta::FromMilliseconds(kLoginFadeoutTransitionDurationMs));
  views::corewm::SetWindowVisibilityAnimationTransition(
      login_window_->GetNativeView(),
      views::corewm::ANIMATE_HIDE);

  login_window_->SetContentsView(login_view_);

  // If WebUI is initialized in hidden state, show it only if we're no
  // longer waiting for wallpaper animation/user images loading. Otherwise,
  // always show it.
  if (!initialize_webui_hidden_ ||
      (!waiting_for_wallpaper_load_ && !waiting_for_user_pods_)) {
    LOG(WARNING) << "Login WebUI >> show login wnd on create";
    login_window_->Show();
  } else {
    LOG(WARNING) << "Login WebUI >> login wnd is hidden on create";
    login_view_->set_is_hidden(true);
  }
  login_window_->GetNativeView()->SetName("WebUILoginView");
}

void LoginDisplayHostImpl::ResetLoginWindowAndView() {
  if (!login_window_)
    return;
  ash::FramePainter::SetSoloWindowHeadersEnabled(true);
  login_window_->Close();
  login_window_ = NULL;
  login_view_ = NULL;
}

bool LoginDisplayHostImpl::IsRunningUserAdding() {
  return restore_path_ == RESTORE_ADD_USER_INTO_SESSION;
}

void LoginDisplayHostImpl::OnAuthPrewarmDone() {
  auth_prewarmer_.reset();
}

void LoginDisplayHostImpl::SetOobeProgressBarVisible(bool visible) {
  GetOobeUI()->ShowOobeUI(visible);
}

////////////////////////////////////////////////////////////////////////////////
// external

// Declared in login_wizard.h so that others don't need to depend on our .h.
// TODO(nkostylev): Split this into a smaller functions.
void ShowLoginWizard(const std::string& first_screen_name) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  VLOG(1) << "Showing OOBE screen: " << first_screen_name;

  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();

  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    manager->SetInputMethodDefault();

    PrefService* prefs = g_browser_process->local_state();
    // Apply owner preferences for tap-to-click and mouse buttons swap for
    // login screen.
    system::mouse_settings::SetPrimaryButtonRight(
        prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight));
    system::touchpad_settings::SetTapToClick(
      prefs->GetBoolean(prefs::kOwnerTapToClickEnabled));
  }

  ui::SetNaturalScroll(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNaturalScrollDefault));

  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));

  // Check whether we need to execute OOBE process.
  bool oobe_complete = chromeos::StartupUtils::IsOobeCompleted();
  if (!oobe_complete) {
    LoginState::Get()->SetLoggedInState(
        LoginState::LOGGED_IN_OOBE, LoginState::LOGGED_IN_USER_NONE);
  } else {
    LoginState::Get()->SetLoggedInState(
        LoginState::LOGGED_IN_NONE, LoginState::LOGGED_IN_USER_NONE);
  }

  LoginDisplayHost* display_host = new LoginDisplayHostImpl(screen_bounds);

  bool show_app_launch_splash_screen = (first_screen_name ==
      chromeos::WizardController::kAppLaunchSplashScreenName);

  if (show_app_launch_splash_screen) {
    const std::string& auto_launch_app_id =
        chromeos::KioskAppManager::Get()->GetAutoLaunchApp();
    display_host->StartAppLaunch(auto_launch_app_id);
    return;
  }

  bool show_login_screen =
      (first_screen_name.empty() && oobe_complete) ||
      first_screen_name == chromeos::WizardController::kLoginScreenName;

  if (show_login_screen) {
    // R11 > R12 migration fix. See http://crosbug.com/p/4898.
    // If user has manually changed locale during R11 OOBE, locale will be set.
    // On R12 > R12|R13 etc. this fix won't get activated since
    // OOBE process has set kApplicationLocale to non-default value.
    PrefService* prefs = g_browser_process->local_state();
    if (!prefs->HasPrefPath(prefs::kApplicationLocale)) {
      std::string locale = chromeos::StartupUtils::GetInitialLocale();
      prefs->SetString(prefs::kApplicationLocale, locale);
      manager->EnableLayouts(
          locale,
          manager->GetInputMethodUtil()->GetHardwareInputMethodId());
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      const std::string loaded_locale =
          ResourceBundle::GetSharedInstance().ReloadLocaleResources(locale);
      g_browser_process->SetApplicationLocale(loaded_locale);
    }
    display_host->StartSignInScreen();
    return;
  }

  // Load startup manifest.
  const chromeos::StartupCustomizationDocument* startup_manifest =
      chromeos::StartupCustomizationDocument::GetInstance();

  // Switch to initial locale if specified by customization
  // and has not been set yet. We cannot call
  // chromeos::LanguageSwitchMenu::SwitchLanguage here before
  // EmitLoginPromptReady.
  PrefService* prefs = g_browser_process->local_state();
  const std::string current_locale =
      prefs->GetString(prefs::kApplicationLocale);
  VLOG(1) << "Current locale: " << current_locale;
  std::string locale;
  if (current_locale.empty()) {
    locale = startup_manifest->initial_locale();
    std::string layout = startup_manifest->keyboard_layout();
    VLOG(1) << "Initial locale: " << locale
            << "keyboard layout " << layout;
    if (!locale.empty()) {
      // Save initial locale from VPD/customization manifest as current
      // Chrome locale. Otherwise it will be lost if Chrome restarts.
      // Don't need to schedule pref save because setting initial local
      // will enforce preference saving.
      prefs->SetString(prefs::kApplicationLocale, locale);
      chromeos::StartupUtils::SetInitialLocale(locale);
      // Determine keyboard layout from OEM customization (if provided) or
      // initial locale and save it in preferences.
      DetermineAndSaveHardwareKeyboard(locale, layout);
      // Then, enable the hardware keyboard.
      manager->EnableLayouts(
          locale,
          manager->GetInputMethodUtil()->GetHardwareInputMethodId());
      // Reloading resource bundle causes us to do blocking IO on UI thread.
      // Temporarily allow it until we fix http://crosbug.com/11102
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      const std::string loaded_locale =
          ResourceBundle::GetSharedInstance().ReloadLocaleResources(locale);
      CHECK(!loaded_locale.empty()) << "Locale could not be found for "
                                    << locale;
      // Set the application locale here so that the language switch
      // menu works properly with the newly loaded locale.
      g_browser_process->SetApplicationLocale(loaded_locale);

      // Reload font settings here to use correct font for initial_locale.
      LanguageSwitchMenu::LoadFontsForCurrentLocale();
    }
  }

  scoped_ptr<DictionaryValue> params;
  display_host->StartWizard(first_screen_name, params.Pass());

  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()
      ->EmitLoginPromptReady();
  TRACE_EVENT0("chromeos", "ShowLoginWizard::EmitLoginPromptReady");

  // Set initial timezone if specified by customization.
  const std::string timezone_name = startup_manifest->initial_timezone();
  VLOG(1) << "Initial time zone: " << timezone_name;
  // Apply locale customizations only once to preserve whatever locale
  // user has changed to during OOBE.
  if (!timezone_name.empty()) {
    chromeos::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
        UTF8ToUTF16(timezone_name));
  }
}

}  // namespace chromeos
