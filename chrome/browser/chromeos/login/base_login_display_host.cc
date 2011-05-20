// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/base_login_display_host.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/views_login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/system_access.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "ui/base/resource/resource_bundle.h"
#include "unicode/timezone.h"

#if defined(TOUCH_UI)
#include "base/command_line.h"
#include "chrome/browser/chromeos/login/dom_login_display_host.h"
#endif

namespace {

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
const int kPolicyServiceInitializationDelayMilliseconds = 100;

// Determines the hardware keyboard from the given locale code
// and the OEM layout information, and saves it to "Locale State".
// The information will be used in input_method::GetHardwareInputMethodId().
void DetermineAndSaveHardwareKeyboard(const std::string& locale,
                                      const std::string& oem_layout) {
  std::string layout;
  if (!oem_layout.empty()) {
    // If the OEM layout information is provided, use it.
    layout = oem_layout;
  } else {
    // Otherwise, determine the hardware keyboard from the locale.
    std::vector<std::string> input_method_ids;
    if (chromeos::input_method::GetInputMethodIdsFromLanguageCode(
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
    prefs->SavePersistentPrefs();
  }
}

}  // namespace

namespace chromeos {

// static
LoginDisplayHost* BaseLoginDisplayHost::default_host_ = NULL;

// BaseLoginDisplayHost --------------------------------------------------------

BaseLoginDisplayHost::BaseLoginDisplayHost(const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds) {
  registrar_.Add(
      this,
      NotificationType::APP_TERMINATING,
      NotificationService::AllSources());
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

// LoginDisplayHost implementation ---------------------------------------------

void BaseLoginDisplayHost::OnSessionStart() {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void BaseLoginDisplayHost::StartWizard(
    const std::string& first_screen_name,
    const GURL& start_url) {
  DVLOG(1) << "Starting wizard, first_screen_name: " << first_screen_name;
  // Create and show the wizard.
  wizard_controller_.reset();  // Only one controller in a time.
  wizard_controller_.reset(new WizardController(this, background_bounds_));
  wizard_controller_->set_start_url(start_url);
  ShowBackground();
  if (!WizardController::IsDeviceRegistered())
    SetOobeProgressBarVisible(true);
  wizard_controller_->Init(first_screen_name);
}

void BaseLoginDisplayHost::StartSignInScreen() {
  DVLOG(1) << "Starting sign in screen";
  std::vector<chromeos::UserManager::User> users =
      chromeos::UserManager::Get()->GetUsers();

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
  ShowBackground();
  SetShutdownButtonEnabled(true);
  sign_in_controller_->Init(users);

  // Initiate services customization manifest fetching.
  ServicesCustomizationDocument::GetInstance()->StartFetching();

  // Initiate device policy fetching.
  g_browser_process->browser_policy_connector()->
      ScheduleServiceInitialization(
          kPolicyServiceInitializationDelayMilliseconds);
}

// BaseLoginDisplayHost --------------------------------------------------------

void BaseLoginDisplayHost::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  CHECK(type == NotificationType::APP_TERMINATING);

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  MessageLoop::current()->Quit();
  registrar_.Remove(this,
                    NotificationType::APP_TERMINATING,
                    NotificationService::AllSources());
}

}  // namespace chromeos

// browser::ShowLoginWizard implementation -------------------------------------

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
// TODO(nkostylev): Split this into a smaller functions.
void ShowLoginWizard(const std::string& first_screen_name,
                     const gfx::Size& size) {
  VLOG(1) << "Showing login screen: " << first_screen_name;

  // The login screen will enable alternate keyboard layouts, but we don't want
  // to start the IME process unless one is selected.
  chromeos::CrosLibrary::Get()->GetInputMethodLibrary()->
      SetDeferImeStartup(true);
  // Tell the window manager that the user isn't logged in.
  chromeos::WmIpc::instance()->SetLoggedInProperty(false);

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
          chromeos::input_method::GetHardwareInputMethodId();
    }
    chromeos::input_method::EnableInputMethods(
        locale, chromeos::input_method::kKeyboardLayoutsOnly,
        initial_input_method_id);
  }

  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(size));

  // Check whether we need to execute OOBE process.
  bool oobe_complete = WizardController::IsOobeCompleted();
  bool show_login_screen =
      (first_screen_name.empty() && oobe_complete) ||
      first_screen_name == WizardController::kLoginScreenName;

  // TODO(nkostylev) Create LoginDisplayHost instance based on flag.
#if defined(TOUCH_UI)
  chromeos::LoginDisplayHost* display_host;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDOMLogin)) {
    display_host = new chromeos::DOMLoginDisplayHost(screen_bounds);
  } else {
    display_host = new chromeos::ViewsLoginDisplayHost(screen_bounds);
  }
#else
  chromeos::LoginDisplayHost* display_host =
      new chromeos::ViewsLoginDisplayHost(screen_bounds);
#endif
  if (show_login_screen && chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    display_host->StartSignInScreen();
    return;
  }

  // Load startup manifest.
  const chromeos::StartupCustomizationDocument* startup_manifest =
      chromeos::StartupCustomizationDocument::GetInstance();

  std::string locale;
  if (startup_manifest->IsReady()) {
    // Switch to initial locale if specified by customization
    // and has not been set yet. We cannot call
    // chromeos::LanguageSwitchMenu::SwitchLanguage here before
    // EmitLoginPromptReady.
    PrefService* prefs = g_browser_process->local_state();
    const std::string current_locale =
        prefs->GetString(prefs::kApplicationLocale);
    VLOG(1) << "Current locale: " << current_locale;
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
        WizardController::SetInitialLocale(locale);
        // Determine keyboard layout from OEM customization (if provided) or
        // initial locale and save it in preferences.
        DetermineAndSaveHardwareKeyboard(locale, layout);
        // Then, enable the hardware keyboard.
        chromeos::input_method::EnableInputMethods(
            locale,
            chromeos::input_method::kKeyboardLayoutsOnly,
            chromeos::input_method::GetHardwareInputMethodId());
        // Reloading resource bundle causes us to do blocking IO on UI thread.
        // Temporarily allow it until we fix http://crosbug.com/11102
        base::ThreadRestrictions::ScopedAllowIO allow_io;
        const std::string loaded_locale =
            ResourceBundle::ReloadSharedInstance(locale);
        CHECK(!loaded_locale.empty()) << "Locale could not be found for "
                                      << locale;
        // Set the application locale here so that the language switch
        // menu works properly with the newly loaded locale.
        g_browser_process->SetApplicationLocale(loaded_locale);
      }
    }
  }

  display_host->StartWizard(first_screen_name, GURL());

  chromeos::LoginUtils::Get()->PrewarmAuthentication();
  if (chromeos::CrosLibrary::Get()->EnsureLoaded())
    chromeos::CrosLibrary::Get()->GetLoginLibrary()->EmitLoginPromptReady();

  if (startup_manifest->IsReady()) {
    // Set initial timezone if specified by customization.
    const std::string timezone_name = startup_manifest->initial_timezone();
    VLOG(1) << "Initial time zone: " << timezone_name;
    // Apply locale customizations only once so preserve whatever locale
    // user has changed to during OOBE.
    if (!timezone_name.empty()) {
      icu::TimeZone* timezone = icu::TimeZone::createTimeZone(
          icu::UnicodeString::fromUTF8(timezone_name));
      CHECK(timezone) << "Timezone could not be set for " << timezone_name;
      chromeos::SystemAccess::GetInstance()->SetTimezone(*timezone);
    }
  }
}

}  // namespace browser
