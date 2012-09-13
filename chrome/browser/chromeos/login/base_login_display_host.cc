// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/base_login_display_host.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/policy/auto_enrollment_client.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

namespace {

// Whether sign in transitions are enabled.
const bool kEnableBackgroundAnimation = false;
const bool kEnableBrowserWindowsOpacityAnimation = true;
const bool kEnableBrowserWindowsTransformAnimation = true;

// Sign in transition timings.
static const int kBackgroundTransitionPauseMs = 100;
static const int kBackgroundTransitionDurationMs = 400;
static const int kBrowserTransitionPauseMs = 750;
static const int kBrowserTransitionDurationMs = 350;

// Parameters for background transform transition.
const float kBackgroundScale = 1.05f;
const int kBackgroundTranslate = -50;

// Parameters for browser transform transition.
const float kBrowserScale = 1.05f;
const int kBrowserTranslate = -50;

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
        chromeos::input_method::InputMethodManager::GetInstance();
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
LoginDisplayHost* BaseLoginDisplayHost::default_host_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// BaseLoginDisplayHost, public

BaseLoginDisplayHost::BaseLoginDisplayHost(const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      ALLOW_THIS_IN_INITIALIZER_LIST(pointer_factory_(this)),
      shutting_down_(false),
      oobe_progress_bar_visible_(false) {
  // We need to listen to CLOSE_ALL_BROWSERS_REQUEST but not APP_TERMINATIN
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
  DCHECK(default_host_ == NULL);
  default_host_ = this;

  // Add a reference count so the message loop won't exit when other
  // message loop clients (e.g. menus) do.
  g_browser_process->AddRefModule();
}

BaseLoginDisplayHost::~BaseLoginDisplayHost() {
  // A browser should already exist when destructor is called since
  // deletion is scheduled with MessageLoop::DeleteSoon() from
  // OnSessionStart(), so the browser will already have incremented
  // the reference count.
  g_browser_process->ReleaseModule();

  default_host_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// BaseLoginDisplayHost, LoginDisplayHost implementation:

void BaseLoginDisplayHost::OnSessionStart() {
  DVLOG(1) << "Session starting";
  if (wizard_controller_.get())
    wizard_controller_->OnSessionStart();
  // Display host is deleted once animation is completed
  // since sign in screen widget has to stay alive.
  StartAnimation();
  ShutdownDisplayHost(false);
}

void BaseLoginDisplayHost::OnCompleteLogin() {
  // Cancelling the |auto_enrollment_client_| now allows it to determine whether
  // its protocol finished before login was complete.
  if (auto_enrollment_client_.get())
    auto_enrollment_client_.release()->CancelAndDeleteSoon();
}

void BaseLoginDisplayHost::StartWizard(
    const std::string& first_screen_name,
    DictionaryValue* screen_parameters) {
  DVLOG(1) << "Starting wizard, first_screen_name: " << first_screen_name;
  // Create and show the wizard.
  // Note, dtor of the old WizardController should be called before ctor of the
  // new one, because "default_controller()" is updated there. So pure "reset()"
  // is done before new controller creation.
  wizard_controller_.reset();
  wizard_controller_.reset(CreateWizardController());

  oobe_progress_bar_visible_ = !WizardController::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  wizard_controller_->Init(first_screen_name, screen_parameters);
}

void BaseLoginDisplayHost::StartSignInScreen() {
  DVLOG(1) << "Starting sign in screen";
  const chromeos::UserList& users = chromeos::UserManager::Get()->GetUsers();

  // Fix for users who updated device and thus never passed register screen.
  // If we already have users, we assume that it is not a second part of
  // OOBE. See http://crosbug.com/6289
  if (!WizardController::IsDeviceRegistered() && !users.empty()) {
    VLOG(1) << "Mark device registered because there are remembered users: "
            << users.size();
    WizardController::MarkDeviceRegistered();
  }

  sign_in_controller_.reset();  // Only one controller in a time.
  sign_in_controller_.reset(new chromeos::ExistingUserController(this));
  oobe_progress_bar_visible_ = !WizardController::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  SetStatusAreaVisible(true);
  SetShutdownButtonEnabled(true);
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
}

void BaseLoginDisplayHost::ResumeSignInScreen() {
  // We only get here after a previous call the StartSignInScreen. That sign-in
  // was successful but was interrupted by an auto-enrollment execution; once
  // auto-enrollment is complete we resume the normal login flow from here.
  DVLOG(1) << "Resuming sign in screen";
  CHECK(sign_in_controller_.get());
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  SetStatusAreaVisible(true);
  SetShutdownButtonEnabled(true);
  sign_in_controller_->ResumeLogin();
}

void BaseLoginDisplayHost::CheckForAutoEnrollment() {
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
      base::Bind(&BaseLoginDisplayHost::OnOwnershipStatusCheckDone,
                 pointer_factory_.GetWeakPtr()));
}

////////////////////////////////////////////////////////////////////////////////
// BaseLoginDisplayHost, content:NotificationObserver implementation:

void BaseLoginDisplayHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST) {
    ShutdownDisplayHost(true);
  } else if (type == chrome::NOTIFICATION_BROWSER_OPENED) {
    OnBrowserCreated();
    registrar_.Remove(this,
                      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                      content::NotificationService::AllSources());
    registrar_.Remove(this,
                      chrome::NOTIFICATION_BROWSER_OPENED,
                      content::NotificationService::AllSources());
  }
}

void BaseLoginDisplayHost::ShutdownDisplayHost(bool post_quit_task) {
  if (shutting_down_)
    return;

  shutting_down_ = true;
  registrar_.RemoveAll();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  if (post_quit_task)
    MessageLoop::current()->Quit();
}

void BaseLoginDisplayHost::StartAnimation() {
  if (ash::Shell::GetContainer(
          ash::Shell::GetPrimaryRootWindow(),
          ash::internal::kShellWindowId_DesktopBackgroundContainer)->
          children().empty()) {
    // If there is no background window, don't perform any animation on the
    // default and background layer because there is nothing behind it.
    return;
  }

  // If we've been explicitly told not to do login animations, we will skip most
  // of them. In particular, we'll avoid animating the background or animating
  // the browser's transform.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool disable_animations = command_line->HasSwitch(
      switches::kDisableLoginAnimations);

  const bool do_background_animation =
      kEnableBackgroundAnimation && !disable_animations;

  const bool do_browser_transform_animation =
      kEnableBrowserWindowsTransformAnimation && !disable_animations;

  const bool do_browser_opacity_animation =
      kEnableBrowserWindowsOpacityAnimation;

  // Background animation.
  if (do_background_animation) {
    ui::Layer* background_layer =
        ash::Shell::GetContainer(
            ash::Shell::GetPrimaryRootWindow(),
            ash::internal::kShellWindowId_DesktopBackgroundContainer)->
                layer();

    ui::Transform background_transform;
    background_transform.SetScale(kBackgroundScale, kBackgroundScale);
    background_transform.SetTranslateX(kBackgroundTranslate);
    background_transform.SetTranslateY(kBackgroundTranslate);
    background_layer->SetTransform(background_transform);

    // Pause
    ui::LayerAnimationElement::AnimatableProperties background_pause_properties;
    background_pause_properties.insert(ui::LayerAnimationElement::TRANSFORM);
    background_layer->GetAnimator()->StartAnimation(
        new ui::LayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                background_pause_properties,
                base::TimeDelta::FromMilliseconds(
                    kBackgroundTransitionPauseMs))));

    ui::ScopedLayerAnimationSettings settings(background_layer->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kBackgroundTransitionDurationMs));
    settings.SetTweenType(ui::Tween::EASE_OUT);
    background_layer->SetTransform(ui::Transform());
  }

  // Browser windows layer opacity and transform animation.
  if (do_browser_transform_animation || do_browser_opacity_animation) {
    ui::Layer* default_container_layer =
        ash::Shell::GetContainer(
            ash::Shell::GetPrimaryRootWindow(),
            ash::internal::kShellWindowId_DefaultContainer)->layer();

    ui::LayerAnimationElement::AnimatableProperties browser_pause_properties;

    // Set the initial opacity and transform.
    if (do_browser_transform_animation) {
      ui::Transform browser_transform;
      browser_transform.SetScale(kBrowserScale, kBrowserScale);
      browser_transform.SetTranslateX(kBrowserTranslate);
      browser_transform.SetTranslateY(kBrowserTranslate);
      default_container_layer->SetTransform(browser_transform);
      browser_pause_properties.insert(ui::LayerAnimationElement::TRANSFORM);
    }

    if (do_browser_opacity_animation) {
      default_container_layer->SetOpacity(0);
      browser_pause_properties.insert(ui::LayerAnimationElement::OPACITY);
    }

    // Pause.
    default_container_layer->GetAnimator()->ScheduleAnimation(
        new ui::LayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                browser_pause_properties,
                base::TimeDelta::FromMilliseconds(kBrowserTransitionPauseMs))));

    ui::ScopedLayerAnimationSettings settings(
        default_container_layer->GetAnimator());

    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kBrowserTransitionDurationMs));

    if (do_browser_opacity_animation) {
      // Should interpolate linearly.
      default_container_layer->SetOpacity(1);
    }

    if (do_browser_transform_animation) {
      settings.SetTweenType(ui::Tween::EASE_OUT);
      default_container_layer->SetTransform(ui::Transform());
    }
  }
}

void BaseLoginDisplayHost::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status,
    bool current_user_is_owner) {
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
        base::Bind(&BaseLoginDisplayHost::OnAutoEnrollmentClientDone,
                   base::Unretained(this))));
    auto_enrollment_client_->Start();
  }
}

void BaseLoginDisplayHost::OnAutoEnrollmentClientDone() {
  bool auto_enroll = auto_enrollment_client_->should_auto_enroll();
  VLOG(1) << "OnAutoEnrollmentClientDone, decision is " << auto_enroll;

  if (auto_enroll)
    ForceAutoEnrollment();
}

void BaseLoginDisplayHost::ForceAutoEnrollment() {
  if (sign_in_controller_.get())
    sign_in_controller_->DoAutoEnrollment();
}

// Declared in login_wizard.h so that others don't need to depend on our .h.
// TODO(nkostylev): Split this into a smaller functions.
void ShowLoginWizard(const std::string& first_screen_name,
                     const gfx::Size& size) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  VLOG(1) << "Showing OOBE screen: " << first_screen_name;

  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();

  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    // If the preferred keyboard for the login screen has been saved, use it.
    PrefService* prefs = g_browser_process->local_state();
    std::string initial_input_method_id =
        prefs->GetString(chromeos::language_prefs::kPreferredKeyboardLayout);
    if (initial_input_method_id.empty()) {
      // If kPreferredKeyboardLayout is not specified, use the hardware layout.
      initial_input_method_id =
          manager->GetInputMethodUtil()->GetHardwareInputMethodId();
    }
    manager->EnableLayouts(locale, initial_input_method_id);

    // Apply owner preferences for tap-to-click and mouse buttons swap for
    // login screen.
    system::mouse_settings::SetPrimaryButtonRight(
        prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight));
    system::touchpad_settings::SetTapToClick(
        prefs->GetBoolean(prefs::kOwnerTapToClickEnabled));
  }

  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(size));

  // Check whether we need to execute OOBE process.
  bool oobe_complete = chromeos::WizardController::IsOobeCompleted();
  bool show_login_screen =
      (first_screen_name.empty() && oobe_complete) ||
      first_screen_name == chromeos::WizardController::kLoginScreenName;

  chromeos::LoginDisplayHost* display_host;
  display_host = new chromeos::WebUILoginDisplayHost(screen_bounds);

  if (show_login_screen) {
    // R11 > R12 migration fix. See http://crosbug.com/p/4898.
    // If user has manually changed locale during R11 OOBE, locale will be set.
    // On R12 > R12|R13 etc. this fix won't get activated since
    // OOBE process has set kApplicationLocale to non-default value.
    PrefService* prefs = g_browser_process->local_state();
    if (!prefs->HasPrefPath(prefs::kApplicationLocale)) {
      std::string locale = chromeos::WizardController::GetInitialLocale();
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
      chromeos::WizardController::SetInitialLocale(locale);
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
    }
  }

  display_host->StartWizard(first_screen_name, NULL);

  chromeos::LoginUtils::Get()->PrewarmAuthentication();
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()
      ->EmitLoginPromptReady();

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
