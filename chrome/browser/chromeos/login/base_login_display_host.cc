// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/base_login_display_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/policy/auto_enrollment_client.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/rect.h"
#include "unicode/timezone.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

#if defined(USE_AURA)
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/window.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_element.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"
#endif

namespace {

// Whether sign in transitions are enabled.
const bool kEnableSigninTransitions = true;
const bool kEnableBackgroundAnimation = false;
const bool kEnableBrowserWindowsOpacityAnimation = true;

// Sign in transition timings.
static const int kLoginFadeoutTransitionDurationMs = 700;
static const int kBackgroundTransitionPauseMs = 100;
static const int kBackgroundTransitionDurationMs = 400;
static const int kBrowserTransitionPauseMs = 750;
static const int kBrowserTransitionDurationMs = 300;

// Parameters for background transform transition.
const float kBackgroundScale = 1.05f;
const int kBackgroundTranslate = -50;

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
const int64 kPolicyServiceInitializationDelayMilliseconds = 100;

// A boolean pref of the auto-enrollment decision. This pref only exists if the
// auto-enrollment check has been done once and completed.
const char kShouldAutoEnroll[] = "ShouldAutoEnroll";

// Returns true if an auto-enrollment decision has been made and is cached in
// local state. If so, the decision is stored in |auto_enroll|.
bool GetAutoEnrollmentDecision(bool* auto_enroll) {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(kShouldAutoEnroll);
  if (pref && !pref->IsDefaultValue())
    return pref->GetValue()->GetAsBoolean(auto_enroll);
  else
    return false;
}

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

#if defined(USE_AURA)
ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}
#endif

}  // namespace

namespace chromeos {

// static
LoginDisplayHost* BaseLoginDisplayHost::default_host_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// BaseLoginDisplayHost, public

BaseLoginDisplayHost::BaseLoginDisplayHost(const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds) {
  // We need to listen to APP_EXITING but not APP_TERMINATING because
  // APP_TERMINATING will never be fired as long as this keeps ref-count.
  // APP_EXITING is safe here because there will be no browser instance that
  // will block the shutdown.
  registrar_.Add(this,
                 content::NOTIFICATION_APP_EXITING,
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

// static
void BaseLoginDisplayHost::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterBooleanPref(kShouldAutoEnroll,
                                   false,
                                   PrefService::UNSYNCABLE_PREF);
}

////////////////////////////////////////////////////////////////////////////////
// BaseLoginDisplayHost, LoginDisplayHost implementation:

void BaseLoginDisplayHost::OnSessionStart() {
  DVLOG(1) << "Session starting";
  // Display host is deleted once animation is completed
  // since sign in screen widget has to stay alive.
#if defined(USE_AURA)
  if (kEnableSigninTransitions)
    StartAnimation();
  else
    ShutdownDisplayHost(false);
#else
  ShutdownDisplayHost(false);
#endif
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

  if (!WizardController::IsDeviceRegistered())
    SetOobeProgressBarVisible(true);
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
  if (!WizardController::IsDeviceRegistered()) {
    SetOobeProgressBarVisible(true);
  }
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
  SetOobeProgressBarVisible(true);
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
  ownership_status_checker_.reset(new OwnershipStatusChecker);
  ownership_status_checker_->Check(base::Bind(
      &BaseLoginDisplayHost::OnOwnershipStatusCheckDone,
      base::Unretained(this)));
}

////////////////////////////////////////////////////////////////////////////////
// BaseLoginDisplayHost, ui::LayerAnimationObserver implementation:

#if defined(USE_AURA)
void BaseLoginDisplayHost::OnLayerAnimationEnded(
    const ui::LayerAnimationSequence* sequence) {
  ShutdownDisplayHost(false);
}

void BaseLoginDisplayHost::OnLayerAnimationAborted(
    const ui::LayerAnimationSequence* sequence) {
}

void BaseLoginDisplayHost::OnLayerAnimationScheduled(
    const ui::LayerAnimationSequence* sequence) {
}
#endif

////////////////////////////////////////////////////////////////////////////////
// BaseLoginDisplayHost, content:NotificationObserver implementation:

void BaseLoginDisplayHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  CHECK(type == content::NOTIFICATION_APP_EXITING);
  ShutdownDisplayHost(true);
}

void BaseLoginDisplayHost::ShutdownDisplayHost(bool post_quit_task) {
#if defined(USE_AURA)
  if (kEnableSigninTransitions && GetWidget())
    GetLayer(GetWidget())->GetAnimator()->RemoveObserver(this);
#endif
  registrar_.RemoveAll();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  if (post_quit_task)
    MessageLoop::current()->Quit();
}

void BaseLoginDisplayHost::StartAnimation() {
#if defined(USE_AURA)
  // Fade out transition for sign in screen.
  // BaseLoginDisplayHost will be deleted this animation is ended.
  ui::Layer* layer = GetLayer(GetWidget());
  layer->GetAnimator()->AddObserver(this);
  ui::LayerAnimator::ScopedSettings signin_animation(layer->GetAnimator());
  signin_animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kLoginFadeoutTransitionDurationMs));
  layer->SetOpacity(0.0f);

  // Background animation.
  if (kEnableBackgroundAnimation) {
    ui::Transform background_transform;
    background_transform.SetScale(kBackgroundScale, kBackgroundScale);
    background_transform.SetTranslateX(kBackgroundTranslate);
    background_transform.SetTranslateY(kBackgroundTranslate);
    scoped_ptr<ui::LayerAnimationElement>
        background_transform_animation_initial(
            ui::LayerAnimationElement::CreateTransformElement(
                background_transform,
                base::TimeDelta()));
    ui::LayerAnimationElement::AnimatableProperties background_pause_properties;
    background_pause_properties.insert(ui::LayerAnimationElement::TRANSFORM);
    scoped_ptr<ui::LayerAnimationElement> background_pause(
        ui::LayerAnimationElement::CreatePauseElement(
            background_pause_properties,
            base::TimeDelta::FromMilliseconds(kBackgroundTransitionPauseMs)));
    scoped_ptr<ui::LayerAnimationElement> background_transform_animation(
        ui::LayerAnimationElement::CreateTransformElement(
            ui::Transform(),
            base::TimeDelta::FromMilliseconds(
                kBackgroundTransitionDurationMs)));
    scoped_ptr<ui::LayerAnimationSequence> background_transition(
        new ui::LayerAnimationSequence(
            background_transform_animation_initial.release()));
    background_transition->AddElement(background_pause.release());
    background_transition->AddElement(background_transform_animation.release());
    ui::Layer* background_layer =
        ash::Shell::GetInstance()->GetContainer(
            ash::internal::kShellWindowId_DesktopBackgroundContainer)->
            layer();
    background_layer->GetAnimator()->StartAnimation(
        background_transition.release());
  }

  // Browser windows layer opacity animation.
  if (kEnableBrowserWindowsOpacityAnimation) {
    scoped_ptr<ui::LayerAnimationElement> browser_opacity_animation_initial(
        ui::LayerAnimationElement::CreateOpacityElement(
            0.0f,
            base::TimeDelta()));
    ui::LayerAnimationElement::AnimatableProperties browser_pause_properties;
    browser_pause_properties.insert(ui::LayerAnimationElement::OPACITY);
    scoped_ptr<ui::LayerAnimationElement> browser_pause_animation(
        ui::LayerAnimationElement::CreatePauseElement(
            browser_pause_properties,
            base::TimeDelta::FromMilliseconds(kBrowserTransitionPauseMs)));
    scoped_ptr<ui::LayerAnimationElement> browser_opacity_animation(
        ui::LayerAnimationElement::CreateOpacityElement(
            1.0f,
            base::TimeDelta::FromMilliseconds(kBrowserTransitionDurationMs)));
    scoped_ptr<ui::LayerAnimationSequence> browser_transition(
        new ui::LayerAnimationSequence(
            browser_opacity_animation_initial.release()));
    browser_transition->AddElement(browser_pause_animation.release());
    browser_transition->AddElement(browser_opacity_animation.release());
    ui::Layer* default_container_layer =
        ash::Shell::GetInstance()->GetContainer(
            ash::internal::kShellWindowId_DefaultContainer)->layer();
    default_container_layer->GetAnimator()->StartAnimation(
        browser_transition.release());
  }
#endif
}

void BaseLoginDisplayHost::OnOwnershipStatusCheckDone(
    OwnershipService::Status status,
    bool current_user_is_owner) {
  if (status != OwnershipService::OWNERSHIP_NONE) {
    // The device is already owned. No need for auto-enrollment checks.
    VLOG(1) << "CheckForAutoEnrollment: device already owned";
    return;
  }

  bool auto_enroll = false;
  if (GetAutoEnrollmentDecision(&auto_enroll)) {
    // The auto-enrollment protocol has executed before and reached a decision,
    // which has been stored in |auto_enroll|.
    VLOG(1) << "CheckForAutoEnrollment: got cached decision: " << auto_enroll;
    if (auto_enroll)
      ForceAutoEnrollment();
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

  // Auto-update might be in progress and might force a reboot. Cache the
  // decision in local_state.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(kShouldAutoEnroll, auto_enroll);
  local_state->CommitPendingWrite();

  if (auto_enroll)
    ForceAutoEnrollment();
}

void BaseLoginDisplayHost::ForceAutoEnrollment() {
  if (sign_in_controller_.get())
    sign_in_controller_->DoAutoEnrollment();
}

}  // namespace chromeos

////////////////////////////////////////////////////////////////////////////////
// browser::ShowLoginWizard implementation:

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
// TODO(nkostylev): Split this into a smaller functions.
void ShowLoginWizard(const std::string& first_screen_name,
                     const gfx::Size& size) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  VLOG(1) << "Showing OOBE screen: " << first_screen_name;

  // The login screen will enable alternate keyboard layouts, but we don't want
  // to start the IME process unless one is selected.
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::GetInstance();
  manager->SetDeferImeStartup(true);

#if defined(TOOLKIT_USES_GTK)
  // Tell the window manager that the user isn't logged in.
  chromeos::WmIpc::instance()->SetLoggedInProperty(false);
#endif

  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    // If the preferred keyboard for the login screen has been saved, use it.
    std::string initial_input_method_id =
        g_browser_process->local_state()->GetString(
            chromeos::language_prefs::kPreferredKeyboardLayout);
    if (initial_input_method_id.empty()) {
      // If kPreferredKeyboardLayout is not specified, use the hardware layout.
      initial_input_method_id =
          manager->GetInputMethodUtil()->GetHardwareInputMethodId();
    }
    manager->EnableInputMethods(
        locale, chromeos::input_method::kKeyboardLayoutsOnly,
        initial_input_method_id);
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
      manager->EnableInputMethods(
          locale,
          chromeos::input_method::kKeyboardLayoutsOnly,
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
      manager->EnableInputMethods(
          locale,
          chromeos::input_method::kKeyboardLayoutsOnly,
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
    icu::TimeZone* timezone = icu::TimeZone::createTimeZone(
        icu::UnicodeString::fromUTF8(timezone_name));
    CHECK(timezone) << "Timezone could not be set for " << timezone_name;
    chromeos::system::TimezoneSettings::GetInstance()->SetTimezone(*timezone);
  }
}

}  // namespace browser
