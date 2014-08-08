// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/geolocation/simple_geolocation_provider.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screens/controller_pairing_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/screens/host_pairing_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/timezone/timezone_provider.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/breakpad/app/breakpad_linux.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/accelerators/accelerator.h"

using content::BrowserThread;

namespace {
// If reboot didn't happen, ask user to reboot device manually.
const int kWaitForRebootTimeSec = 3;

// Interval in ms which is used for smooth screen showing.
static int kShowDelayMs = 400;

// Total timezone resolving process timeout.
const unsigned int kResolveTimeZoneTimeoutSeconds = 60;

// Stores the list of all screens that should be shown when resuming OOBE.
const char *kResumableScreens[] = {
  chromeos::WizardController::kNetworkScreenName,
  chromeos::WizardController::kUpdateScreenName,
  chromeos::WizardController::kEulaScreenName,
  chromeos::WizardController::kEnrollmentScreenName,
  chromeos::WizardController::kTermsOfServiceScreenName,
  chromeos::WizardController::kAutoEnrollmentCheckScreenName
};

// Checks flag for HID-detection screen show.
bool CanShowHIDDetectionScreen() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
        chromeos::switches::kDisableHIDDetectionOnOOBE);
}

bool ShouldShowControllerPairingScreen() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kShowControllerPairingDemo);
}

bool ShouldShowHostPairingScreen() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kShowHostPairingDemo);
}

bool IsResumableScreen(const std::string& screen) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kResumableScreens); ++i) {
    if (screen == kResumableScreens[i])
      return true;
  }
  return false;
}

void RecordUMAHistogramForOOBEStepCompletionTime(std::string screen_name,
                                                 base::TimeDelta step_time) {
  screen_name[0] = std::toupper(screen_name[0]);
  std::string histogram_name = "OOBE.StepCompletionTime." + screen_name;
  // Equivalent to using UMA_HISTOGRAM_MEDIUM_TIMES. UMA_HISTOGRAM_MEDIUM_TIMES
  // can not be used here, because |histogram_name| is calculated dynamically
  // and changes from call to call.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name,
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMinutes(3),
      50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(step_time);
}

}  // namespace

namespace chromeos {

const char WizardController::kNetworkScreenName[] = "network";
const char WizardController::kLoginScreenName[] = "login";
const char WizardController::kUpdateScreenName[] = "update";
const char WizardController::kUserImageScreenName[] = "image";
const char WizardController::kEulaScreenName[] = "eula";
const char WizardController::kEnrollmentScreenName[] = "enroll";
const char WizardController::kResetScreenName[] = "reset";
const char WizardController::kKioskEnableScreenName[] = "kiosk-enable";
const char WizardController::kKioskAutolaunchScreenName[] = "autolaunch";
const char WizardController::kErrorScreenName[] = "error-message";
const char WizardController::kTermsOfServiceScreenName[] = "tos";
const char WizardController::kAutoEnrollmentCheckScreenName[] =
  "auto-enrollment-check";
const char WizardController::kWrongHWIDScreenName[] = "wrong-hwid";
const char WizardController::kSupervisedUserCreationScreenName[] =
  "supervised-user-creation-flow";
const char WizardController::kAppLaunchSplashScreenName[] =
  "app-launch-splash";
const char WizardController::kHIDDetectionScreenName[] = "hid-detection";
const char WizardController::kControllerPairingScreenName[] =
    "controller-pairing";
const char WizardController::kHostPairingScreenName[] = "host-pairing";

// static
const int WizardController::kMinAudibleOutputVolumePercent = 10;

// Passing this parameter as a "first screen" initiates full OOBE flow.
const char WizardController::kOutOfBoxScreenName[] = "oobe";

// Special test value that commands not to create any window yet.
const char WizardController::kTestNoScreenName[] = "test:nowindow";

// Initialize default controller.
// static
WizardController* WizardController::default_controller_ = NULL;

// static
bool WizardController::skip_post_login_screens_ = false;

// static
bool WizardController::zero_delay_enabled_ = false;

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:

PrefService* WizardController::local_state_for_testing_ = NULL;

WizardController::WizardController(chromeos::LoginDisplayHost* host,
                                   chromeos::OobeDisplay* oobe_display)
    : current_screen_(NULL),
      previous_screen_(NULL),
#if defined(GOOGLE_CHROME_BUILD)
      is_official_build_(true),
#else
      is_official_build_(false),
#endif
      is_out_of_box_(false),
      host_(host),
      oobe_display_(oobe_display),
      usage_statistics_reporting_(true),
      skip_update_enroll_after_eula_(false),
      enrollment_recovery_(ShouldRecoverEnrollment()),
      login_screen_started_(false),
      user_image_screen_return_to_previous_hack_(false),
      timezone_resolved_(false),
      weak_factory_(this) {
  DCHECK(default_controller_ == NULL);
  default_controller_ = this;
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&WizardController::OnAccessibilityStatusChanged,
                 base::Unretained(this)));
}

WizardController::~WizardController() {
  if (default_controller_ == this) {
    default_controller_ = NULL;
  } else {
    NOTREACHED() << "More than one controller are alive.";
  }
}

void WizardController::Init(
    const std::string& first_screen_name,
    scoped_ptr<base::DictionaryValue> screen_parameters) {
  VLOG(1) << "Starting OOBE wizard with screen: " << first_screen_name;
  first_screen_name_ = first_screen_name;
  screen_parameters_ = screen_parameters.Pass();

  bool oobe_complete = StartupUtils::IsOobeCompleted();
  if (!oobe_complete || first_screen_name == kOutOfBoxScreenName)
    is_out_of_box_ = true;

  // This is a hacky way to check for local state corruption, because
  // it depends on the fact that the local state is loaded
  // synchroniously and at the first demand. IsEnterpriseManaged()
  // check is required because currently powerwash is disabled for
  // enterprise-entrolled devices.
  //
  // TODO (ygorshenin@): implement handling of the local state
  // corruption in the case of asynchronious loading.
  //
  // TODO (ygorshenin@): remove IsEnterpriseManaged() check once
  // crbug.com/241313 will be fixed.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged()) {
    const PrefService::PrefInitializationStatus status =
        GetLocalState()->GetInitializationStatus();
    if (status == PrefService::INITIALIZATION_STATUS_ERROR) {
      OnLocalStateInitialized(false);
      return;
    } else if (status == PrefService::INITIALIZATION_STATUS_WAITING) {
      GetLocalState()->AddPrefInitObserver(
          base::Bind(&WizardController::OnLocalStateInitialized,
                     weak_factory_.GetWeakPtr()));
    }
  }

  const std::string screen_pref =
      GetLocalState()->GetString(prefs::kOobeScreenPending);
  if (is_out_of_box_ && !screen_pref.empty() && (first_screen_name.empty() ||
      first_screen_name == WizardController::kTestNoScreenName)) {
    first_screen_name_ = screen_pref;
  }

  AdvanceToScreen(first_screen_name_);
  if (!IsMachineHWIDCorrect() && !StartupUtils::IsDeviceRegistered() &&
      first_screen_name_.empty())
    ShowWrongHWIDScreen();
}

chromeos::NetworkScreen* WizardController::GetNetworkScreen() {
  if (!network_screen_.get())
    network_screen_.reset(new chromeos::NetworkScreen(
        this, oobe_display_->GetNetworkScreenActor()));
  return network_screen_.get();
}

chromeos::UpdateScreen* WizardController::GetUpdateScreen() {
  if (!update_screen_.get()) {
    update_screen_.reset(new chromeos::UpdateScreen(
        this, oobe_display_->GetUpdateScreenActor()));
    update_screen_->SetRebootCheckDelay(kWaitForRebootTimeSec);
  }
  return update_screen_.get();
}

chromeos::UserImageScreen* WizardController::GetUserImageScreen() {
  if (!user_image_screen_.get())
    user_image_screen_.reset(
        new chromeos::UserImageScreen(
            this, oobe_display_->GetUserImageScreenActor()));
  return user_image_screen_.get();
}

chromeos::EulaScreen* WizardController::GetEulaScreen() {
  if (!eula_screen_.get())
    eula_screen_.reset(new chromeos::EulaScreen(
        this, oobe_display_->GetEulaScreenActor()));
  return eula_screen_.get();
}

chromeos::EnrollmentScreen*
    WizardController::GetEnrollmentScreen() {
  if (!enrollment_screen_.get()) {
    enrollment_screen_.reset(
        new chromeos::EnrollmentScreen(
            this, oobe_display_->GetEnrollmentScreenActor()));
  }
  return enrollment_screen_.get();
}

chromeos::ResetScreen* WizardController::GetResetScreen() {
  if (!reset_screen_.get()) {
    reset_screen_.reset(
        new chromeos::ResetScreen(this, oobe_display_->GetResetScreenActor()));
  }
  return reset_screen_.get();
}

chromeos::KioskEnableScreen* WizardController::GetKioskEnableScreen() {
  if (!kiosk_enable_screen_.get()) {
    kiosk_enable_screen_.reset(
        new chromeos::KioskEnableScreen(
            this,
            oobe_display_->GetKioskEnableScreenActor()));
  }
  return kiosk_enable_screen_.get();
}

chromeos::KioskAutolaunchScreen* WizardController::GetKioskAutolaunchScreen() {
  if (!autolaunch_screen_.get()) {
    autolaunch_screen_.reset(
        new chromeos::KioskAutolaunchScreen(
            this, oobe_display_->GetKioskAutolaunchScreenActor()));
  }
  return autolaunch_screen_.get();
}

chromeos::TermsOfServiceScreen* WizardController::GetTermsOfServiceScreen() {
  if (!terms_of_service_screen_.get()) {
    terms_of_service_screen_.reset(
        new chromeos::TermsOfServiceScreen(
            this, oobe_display_->GetTermsOfServiceScreenActor()));
  }
  return terms_of_service_screen_.get();
}

chromeos::WrongHWIDScreen* WizardController::GetWrongHWIDScreen() {
  if (!wrong_hwid_screen_.get()) {
    wrong_hwid_screen_.reset(
        new chromeos::WrongHWIDScreen(
            this, oobe_display_->GetWrongHWIDScreenActor()));
  }
  return wrong_hwid_screen_.get();
}

chromeos::AutoEnrollmentCheckScreen*
    WizardController::GetAutoEnrollmentCheckScreen() {
  if (!auto_enrollment_check_screen_.get()) {
    auto_enrollment_check_screen_.reset(
        new chromeos::AutoEnrollmentCheckScreen(
            this,
            oobe_display_->GetAutoEnrollmentCheckScreenActor()));
  }
  return auto_enrollment_check_screen_.get();
}

chromeos::SupervisedUserCreationScreen*
    WizardController::GetSupervisedUserCreationScreen() {
  if (!supervised_user_creation_screen_.get()) {
    supervised_user_creation_screen_.reset(
        new chromeos::SupervisedUserCreationScreen(
            this, oobe_display_->GetSupervisedUserCreationScreenActor()));
  }
  return supervised_user_creation_screen_.get();
}

chromeos::HIDDetectionScreen* WizardController::GetHIDDetectionScreen() {
  if (!hid_detection_screen_.get()) {
    hid_detection_screen_.reset(
        new chromeos::HIDDetectionScreen(
            this, oobe_display_->GetHIDDetectionScreenActor()));
  }
  return hid_detection_screen_.get();
}

ControllerPairingScreen* WizardController::GetControllerPairingScreen() {
  if (!controller_pairing_screen_) {
    controller_pairing_screen_.reset(new ControllerPairingScreen(
        this, oobe_display_->GetControllerPairingScreenActor()));
  }
  return controller_pairing_screen_.get();
}

HostPairingScreen* WizardController::GetHostPairingScreen() {
  if (!host_pairing_screen_) {
    host_pairing_screen_.reset(new HostPairingScreen(
        this, oobe_display_->GetHostPairingScreenActor()));
  }
  return host_pairing_screen_.get();
}

void WizardController::ShowNetworkScreen() {
  VLOG(1) << "Showing network screen.";
  // Hide the status area initially; it only appears after OOBE first animates
  // in. Keep it visible if the user goes back to the existing network screen.
  SetStatusAreaVisible(network_screen_.get());
  SetCurrentScreen(GetNetworkScreen());
}

void WizardController::ShowLoginScreen(const LoginScreenContext& context) {
  if (!time_eula_accepted_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_eula_accepted_;
    UMA_HISTOGRAM_MEDIUM_TIMES("OOBE.EULAToSignInTime", delta);
  }
  VLOG(1) << "Showing login screen.";
  SetStatusAreaVisible(true);
  host_->StartSignInScreen(context);
  smooth_show_timer_.Stop();
  oobe_display_ = NULL;
  login_screen_started_ = true;
}

void WizardController::ResumeLoginScreen() {
  VLOG(1) << "Resuming login screen.";
  SetStatusAreaVisible(true);
  host_->ResumeSignInScreen();
  smooth_show_timer_.Stop();
  oobe_display_ = NULL;
}

void WizardController::ShowUpdateScreen() {
  VLOG(1) << "Showing update screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetUpdateScreen());
}

void WizardController::ShowUserImageScreen() {
  const chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  // Skip user image selection for public sessions and ephemeral logins.
  if (user_manager->IsLoggedInAsPublicAccount() ||
      user_manager->IsCurrentUserNonCryptohomeDataEphemeral()) {
    OnUserImageSkipped();
    return;
  }
  VLOG(1) << "Showing user image screen.";

  bool profile_picture_enabled = true;
  std::string user_id;
  if (screen_parameters_.get()) {
    screen_parameters_->GetBoolean("profile_picture_enabled",
        &profile_picture_enabled);
    screen_parameters_->GetString("user_id", &user_id);
  }

  // Status area has been already shown at sign in screen so it
  // doesn't make sense to hide it here and then show again at user session as
  // this produces undesired UX transitions.
  SetStatusAreaVisible(true);

  UserImageScreen* screen = GetUserImageScreen();
  if (!user_id.empty())
    screen->SetUserID(user_id);
  screen->SetProfilePictureEnabled(profile_picture_enabled);

  SetCurrentScreen(screen);
}

void WizardController::ShowEulaScreen() {
  VLOG(1) << "Showing EULA screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetEulaScreen());
}

void WizardController::ShowEnrollmentScreen() {
  VLOG(1) << "Showing enrollment screen.";

  SetStatusAreaVisible(true);

  bool is_auto_enrollment = false;
  std::string user;
  if (screen_parameters_.get()) {
    screen_parameters_->GetBoolean("is_auto_enrollment", &is_auto_enrollment);
    screen_parameters_->GetString("user", &user);
  }

  EnrollmentScreenActor::EnrollmentMode mode =
      EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL;
  EnrollmentScreen* screen = GetEnrollmentScreen();
  std::string enrollment_domain = GetForcedEnrollmentDomain();
  if (is_auto_enrollment) {
    mode = EnrollmentScreenActor::ENROLLMENT_MODE_AUTO;
  } else if (enrollment_recovery_) {
    mode = EnrollmentScreenActor::ENROLLMENT_MODE_RECOVERY;
    enrollment_domain = GetEnrollmentRecoveryDomain();
  } else if (ShouldAutoStartEnrollment() && !CanExitEnrollment()) {
    mode = EnrollmentScreenActor::ENROLLMENT_MODE_FORCED;
  }

  screen->SetParameters(mode, enrollment_domain, user);
  SetCurrentScreen(screen);
}

void WizardController::ShowResetScreen() {
  VLOG(1) << "Showing reset screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetResetScreen());
}

void WizardController::ShowKioskEnableScreen() {
  VLOG(1) << "Showing kiosk enable screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetKioskEnableScreen());
}

void WizardController::ShowKioskAutolaunchScreen() {
  VLOG(1) << "Showing kiosk autolaunch screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetKioskAutolaunchScreen());
}

void WizardController::ShowTermsOfServiceScreen() {
  // Only show the Terms of Service when logging into a public account and Terms
  // of Service have been specified through policy. In all other cases, advance
  // to the user image screen immediately.
  if (!chromeos::UserManager::Get()->IsLoggedInAsPublicAccount() ||
      !ProfileManager::GetActiveUserProfile()->GetPrefs()->
          IsManagedPreference(prefs::kTermsOfServiceURL)) {
    ShowUserImageScreen();
    return;
  }

  VLOG(1) << "Showing Terms of Service screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetTermsOfServiceScreen());
}

void WizardController::ShowWrongHWIDScreen() {
  VLOG(1) << "Showing wrong HWID screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetWrongHWIDScreen());
}

void WizardController::ShowAutoEnrollmentCheckScreen() {
  VLOG(1) << "Showing Auto-enrollment check screen.";
  SetStatusAreaVisible(true);
  AutoEnrollmentCheckScreen* screen = GetAutoEnrollmentCheckScreen();
  screen->set_auto_enrollment_controller(host_->GetAutoEnrollmentController());
  SetCurrentScreen(screen);
}

void WizardController::ShowSupervisedUserCreationScreen() {
  VLOG(1) << "Showing Locally managed user creation screen screen.";
  SetStatusAreaVisible(true);
  SupervisedUserCreationScreen* screen =
      GetSupervisedUserCreationScreen();
  SetCurrentScreen(screen);
}

void WizardController::ShowHIDDetectionScreen() {
  VLOG(1) << "Showing HID discovery screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetHIDDetectionScreen());
}

void WizardController::ShowControllerPairingScreen() {
  VLOG(1) << "Showing controller pairing screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetControllerPairingScreen());
}

void WizardController::ShowHostPairingScreen() {
  VLOG(1) << "Showing host pairing screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetHostPairingScreen());
}

void WizardController::SkipToLoginForTesting(
    const LoginScreenContext& context) {
  VLOG(1) << "SkipToLoginForTesting.";
  StartupUtils::MarkEulaAccepted();
  PerformPostEulaActions();
  OnAutoEnrollmentCheckCompleted();
}

void WizardController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void WizardController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void WizardController::OnSessionStart() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnSessionStart());
}

void WizardController::SkipUpdateEnrollAfterEula() {
  skip_update_enroll_after_eula_ = true;
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnHIDDetectionCompleted() {
  // Check for tests configuration.
  if (!StartupUtils::IsOobeCompleted())
    ShowNetworkScreen();
}

void WizardController::OnNetworkConnected() {
  if (is_official_build_) {
    if (!StartupUtils::IsEulaAccepted()) {
      ShowEulaScreen();
    } else {
      // Possible cases:
      // 1. EULA was accepted, forced shutdown/reboot during update.
      // 2. EULA was accepted, planned reboot after update.
      // Make sure that device is up-to-date.
      InitiateOOBEUpdate();
    }
  } else {
    InitiateOOBEUpdate();
  }
}

void WizardController::OnNetworkOffline() {
  // TODO(dpolukhin): if(is_out_of_box_) we cannot work offline and
  // should report some error message here and stay on the same screen.
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnConnectionFailed() {
  // TODO(dpolukhin): show error message after login screen is displayed.
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnUpdateCompleted() {
  // TODO(dzhioev): place checks related to pairing in a proper place.
  if (ShouldShowControllerPairingScreen()) {
    ShowControllerPairingScreen();
  } else if (ShouldShowHostPairingScreen()) {
    ShowHostPairingScreen();
  } else {
    ShowAutoEnrollmentCheckScreen();
  }
}

void WizardController::OnEulaAccepted() {
  time_eula_accepted_ = base::Time::Now();
  StartupUtils::MarkEulaAccepted();
  bool uma_enabled =
      ResolveMetricsReportingEnabled(usage_statistics_reporting_);

  CrosSettings::Get()->SetBoolean(kStatsReportingPref, uma_enabled);
  if (uma_enabled) {
#if defined(GOOGLE_CHROME_BUILD)
    // The crash reporter initialization needs IO to complete.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    breakpad::InitCrashReporter(std::string());
#endif
  }

  if (skip_update_enroll_after_eula_) {
    PerformPostEulaActions();
    ShowAutoEnrollmentCheckScreen();
  } else {
    InitiateOOBEUpdate();
  }
}

void WizardController::OnUpdateErrorCheckingForUpdate() {
  // TODO(nkostylev): Update should be required during OOBE.
  // We do not want to block users from being able to proceed to the login
  // screen if there is any error checking for an update.
  // They could use "browse without sign-in" feature to set up the network to be
  // able to perform the update later.
  OnUpdateCompleted();
}

void WizardController::OnUpdateErrorUpdating() {
  // If there was an error while getting or applying the update,
  // return to network selection screen.
  // TODO(nkostylev): Show message to the user explaining update error.
  // TODO(nkostylev): Update should be required during OOBE.
  // Temporary fix, need to migrate to new API. http://crosbug.com/4321
  OnUpdateCompleted();
}

void WizardController::EnableUserImageScreenReturnToPreviousHack() {
  user_image_screen_return_to_previous_hack_ = true;
}

void WizardController::OnUserImageSelected() {
  if (user_image_screen_return_to_previous_hack_) {
    user_image_screen_return_to_previous_hack_ = false;
    DCHECK(previous_screen_);
    if (previous_screen_) {
      SetCurrentScreen(previous_screen_);
      return;
    }
  }
  if (!time_oobe_started_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_oobe_started_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "OOBE.BootToSignInCompleted",
        delta,
        base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromMinutes(30),
        100);
    time_oobe_started_ = base::Time();
  }

  // Launch browser and delete login host controller.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&chromeos::LoginUtils::DoBrowserLaunch,
                 base::Unretained(chromeos::LoginUtils::Get()),
                 ProfileManager::GetActiveUserProfile(), host_));
  host_ = NULL;
}

void WizardController::OnUserImageSkipped() {
  OnUserImageSelected();
}

void WizardController::OnEnrollmentDone() {
  // Mark OOBE as completed only if enterprise enrollment was part of the
  // forced flow (i.e. app kiosk).
  if (ShouldAutoStartEnrollment() || enrollment_recovery_)
    PerformOOBECompletedActions();

  // TODO(mnissler): Unify the logic for auto-login for Public Sessions and
  // Kiosk Apps and make this code cover both cases: http://crbug.com/234694.
  if (KioskAppManager::Get()->IsAutoLaunchEnabled())
    AutoLaunchKioskApp();
  else
    ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnResetCanceled() {
  if (previous_screen_) {
    SetCurrentScreen(previous_screen_);
  } else {
    ShowLoginScreen(LoginScreenContext());
  }
}

void WizardController::OnKioskAutolaunchCanceled() {
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnKioskAutolaunchConfirmed() {
  DCHECK(KioskAppManager::Get()->IsAutoLaunchEnabled());
  AutoLaunchKioskApp();
}

void WizardController::OnKioskEnableCompleted() {
  ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnWrongHWIDWarningSkipped() {
  if (previous_screen_)
    SetCurrentScreen(previous_screen_);
  else
    ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnAutoEnrollmentDone() {
  VLOG(1) << "Automagic enrollment done, resuming previous signin";
  ResumeLoginScreen();
}

void WizardController::OnAutoEnrollmentCheckCompleted() {
  if (ShouldAutoStartEnrollment() || enrollment_recovery_) {
    ShowEnrollmentScreen();
  } else {
    PerformOOBECompletedActions();
    ShowLoginScreen(LoginScreenContext());
  }
}

void WizardController::OnTermsOfServiceDeclined() {
  // If the user declines the Terms of Service, end the session and return to
  // the login screen.
  DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
}

void WizardController::OnTermsOfServiceAccepted() {
  // If the user accepts the Terms of Service, advance to the user image screen.
  ShowUserImageScreen();
}

void WizardController::OnControllerPairingFinished() {
  ShowAutoEnrollmentCheckScreen();
}

void WizardController::OnHostPairingFinished() {
  ShowAutoEnrollmentCheckScreen();
}

void WizardController::InitiateOOBEUpdate() {
  PerformPostEulaActions();
  SetCurrentScreenSmooth(GetUpdateScreen(), true);
  GetUpdateScreen()->StartNetworkCheck();
}

void WizardController::StartTimezoneResolve() {
  geolocation_provider_.reset(new SimpleGeolocationProvider(
      g_browser_process->system_request_context(),
      SimpleGeolocationProvider::DefaultGeolocationProviderURL()));
  geolocation_provider_->RequestGeolocation(
      base::TimeDelta::FromSeconds(kResolveTimeZoneTimeoutSeconds),
      base::Bind(&WizardController::OnLocationResolved,
                 weak_factory_.GetWeakPtr()));
}

void WizardController::PerformPostEulaActions() {
  DelayNetworkCall(
      base::Bind(&WizardController::StartTimezoneResolve,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS));
  DelayNetworkCall(
      ServicesCustomizationDocument::GetInstance()
          ->EnsureCustomizationAppliedClosure(),
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS));

  // Now that EULA has been accepted (for official builds), enable portal check.
  // ChromiumOS builds would go though this code path too.
  NetworkHandler::Get()->network_state_handler()->SetCheckPortalList(
      NetworkStateHandler::kDefaultCheckPortalList);
  host_->GetAutoEnrollmentController()->Start();
  host_->PrewarmAuthentication();
  NetworkPortalDetector::Get()->Enable(true);
}

void WizardController::PerformOOBECompletedActions() {
  UMA_HISTOGRAM_COUNTS_100(
      "HIDDetection.TimesDialogShownPerOOBECompleted",
      GetLocalState()->GetInteger(prefs::kTimesHIDDialogShown));
  GetLocalState()->ClearPref(prefs::kTimesHIDDialogShown);
  StartupUtils::MarkOobeCompleted();

  if (enrollment_recovery_)
    chrome::AttemptRestart();
}

void WizardController::SetCurrentScreen(WizardScreen* new_current) {
  SetCurrentScreenSmooth(new_current, false);
}

void WizardController::ShowCurrentScreen() {
  // ShowCurrentScreen may get called by smooth_show_timer_ even after
  // flow has been switched to sign in screen (ExistingUserController).
  if (!oobe_display_)
    return;

  // First remember how far have we reached so that we can resume if needed.
  if (is_out_of_box_ && IsResumableScreen(current_screen_->GetName()))
    StartupUtils::SaveOobePendingScreen(current_screen_->GetName());

  smooth_show_timer_.Stop();

  FOR_EACH_OBSERVER(Observer, observer_list_, OnScreenChanged(current_screen_));

  current_screen_->Show();
}

void WizardController::SetCurrentScreenSmooth(WizardScreen* new_current,
                                              bool use_smoothing) {
  if (current_screen_ == new_current ||
      new_current == NULL ||
      oobe_display_ == NULL) {
    return;
  }

  smooth_show_timer_.Stop();

  if (current_screen_)
    current_screen_->Hide();

  std::string screen_id = new_current->GetName();
  if (IsOOBEStepToTrack(screen_id))
    screen_show_times_[screen_id] = base::Time::Now();

  previous_screen_ = current_screen_;
  current_screen_ = new_current;

  if (use_smoothing) {
    smooth_show_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kShowDelayMs),
        this,
        &WizardController::ShowCurrentScreen);
  } else {
    ShowCurrentScreen();
  }
}

void WizardController::SetStatusAreaVisible(bool visible) {
  host_->SetStatusAreaVisible(visible);
}

void WizardController::OnHIDScreenNecessityCheck(bool screen_needed) {
  if (!oobe_display_)
    return;
  if (screen_needed)
    ShowHIDDetectionScreen();
  else
    ShowNetworkScreen();
}

void WizardController::AdvanceToScreen(const std::string& screen_name) {
  if (screen_name == kNetworkScreenName) {
    ShowNetworkScreen();
  } else if (screen_name == kLoginScreenName) {
    ShowLoginScreen(LoginScreenContext());
  } else if (screen_name == kUpdateScreenName) {
    InitiateOOBEUpdate();
  } else if (screen_name == kUserImageScreenName) {
    ShowUserImageScreen();
  } else if (screen_name == kEulaScreenName) {
    ShowEulaScreen();
  } else if (screen_name == kResetScreenName) {
    ShowResetScreen();
  } else if (screen_name == kKioskEnableScreenName) {
    ShowKioskEnableScreen();
  } else if (screen_name == kKioskAutolaunchScreenName) {
    ShowKioskAutolaunchScreen();
  } else if (screen_name == kEnrollmentScreenName) {
    ShowEnrollmentScreen();
  } else if (screen_name == kTermsOfServiceScreenName) {
    ShowTermsOfServiceScreen();
  } else if (screen_name == kWrongHWIDScreenName) {
    ShowWrongHWIDScreen();
  } else if (screen_name == kAutoEnrollmentCheckScreenName) {
    ShowAutoEnrollmentCheckScreen();
  } else if (screen_name == kSupervisedUserCreationScreenName) {
    ShowSupervisedUserCreationScreen();
  } else if (screen_name == kAppLaunchSplashScreenName) {
    AutoLaunchKioskApp();
  } else if (screen_name == kHIDDetectionScreenName) {
    ShowHIDDetectionScreen();
  } else if (screen_name == kControllerPairingScreenName) {
    ShowControllerPairingScreen();
  } else if (screen_name == kHostPairingScreenName) {
    ShowHostPairingScreen();
  } else if (screen_name != kTestNoScreenName) {
    if (is_out_of_box_) {
      time_oobe_started_ = base::Time::Now();
      if (CanShowHIDDetectionScreen()) {
        base::Callback<void(bool)> on_check = base::Bind(
            &WizardController::OnHIDScreenNecessityCheck,
            weak_factory_.GetWeakPtr());
        oobe_display_->GetHIDDetectionScreenActor()->CheckIsScreenRequired(
            on_check);
      } else {
        ShowNetworkScreen();
      }
    } else {
      ShowLoginScreen(LoginScreenContext());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, chromeos::ScreenObserver overrides:
void WizardController::OnExit(ExitCodes exit_code) {
  VLOG(1) << "Wizard screen exit code: " << exit_code;
  std::string previous_screen_id = current_screen_->GetName();
  if (IsOOBEStepToTrack(previous_screen_id)) {
    RecordUMAHistogramForOOBEStepCompletionTime(
        previous_screen_id,
        base::Time::Now() - screen_show_times_[previous_screen_id]);
  }
  switch (exit_code) {
    case HID_DETECTION_COMPLETED:
      OnHIDDetectionCompleted();
      break;
    case NETWORK_CONNECTED:
      OnNetworkConnected();
      break;
    case CONNECTION_FAILED:
      OnConnectionFailed();
      break;
    case UPDATE_INSTALLED:
    case UPDATE_NOUPDATE:
      OnUpdateCompleted();
      break;
    case UPDATE_ERROR_CHECKING_FOR_UPDATE:
      OnUpdateErrorCheckingForUpdate();
      break;
    case UPDATE_ERROR_UPDATING:
      OnUpdateErrorUpdating();
      break;
    case USER_IMAGE_SELECTED:
      OnUserImageSelected();
      break;
    case EULA_ACCEPTED:
      OnEulaAccepted();
      break;
    case EULA_BACK:
      ShowNetworkScreen();
      break;
    case ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED:
      if (skip_update_enroll_after_eula_)
        ShowEnrollmentScreen();
      else
        OnAutoEnrollmentCheckCompleted();
      break;
    case ENTERPRISE_ENROLLMENT_COMPLETED:
      OnEnrollmentDone();
      break;
    case ENTERPRISE_ENROLLMENT_BACK:
      ShowNetworkScreen();
      break;
    case RESET_CANCELED:
      OnResetCanceled();
      break;
    case KIOSK_AUTOLAUNCH_CANCELED:
      OnKioskAutolaunchCanceled();
      break;
    case KIOSK_AUTOLAUNCH_CONFIRMED:
      OnKioskAutolaunchConfirmed();
      break;
    case KIOSK_ENABLE_COMPLETED:
      OnKioskEnableCompleted();
      break;
    case ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED:
      OnAutoEnrollmentDone();
      break;
    case TERMS_OF_SERVICE_DECLINED:
      OnTermsOfServiceDeclined();
      break;
    case TERMS_OF_SERVICE_ACCEPTED:
      OnTermsOfServiceAccepted();
      break;
    case WRONG_HWID_WARNING_SKIPPED:
      OnWrongHWIDWarningSkipped();
      break;
    case CONTROLLER_PAIRING_FINISHED:
      OnControllerPairingFinished();
      break;
    case HOST_PAIRING_FINISHED:
      OnHostPairingFinished();
      break;
    default:
      NOTREACHED();
  }
}

void WizardController::OnSetUserNamePassword(const std::string& username,
                                             const std::string& password) {
  username_ = username;
  password_ = password;
}

void WizardController::SetUsageStatisticsReporting(bool val) {
  usage_statistics_reporting_ = val;
}

bool WizardController::GetUsageStatisticsReporting() const {
  return usage_statistics_reporting_;
}

chromeos::ErrorScreen* WizardController::GetErrorScreen() {
  if (!error_screen_.get()) {
    error_screen_.reset(
        new chromeos::ErrorScreen(this, oobe_display_->GetErrorScreenActor()));
  }
  return error_screen_.get();
}

void WizardController::ShowErrorScreen() {
  VLOG(1) << "Showing error screen.";
  SetCurrentScreen(GetErrorScreen());
}

void WizardController::HideErrorScreen(WizardScreen* parent_screen) {
  DCHECK(parent_screen);
  VLOG(1) << "Hiding error screen.";
  SetCurrentScreen(parent_screen);
}

void WizardController::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  enum AccessibilityNotificationType type = details.notification_type;
  if (type == ACCESSIBILITY_MANAGER_SHUTDOWN) {
    accessibility_subscription_.reset();
    return;
  } else if (type != ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK || !details.enabled) {
    return;
  }

  CrasAudioHandler* cras = CrasAudioHandler::Get();
  if (cras->IsOutputMuted()) {
    cras->SetOutputMute(false);
    cras->SetOutputVolumePercent(kMinAudibleOutputVolumePercent);
  } else if (cras->GetOutputVolumePercent() < kMinAudibleOutputVolumePercent) {
    cras->SetOutputVolumePercent(kMinAudibleOutputVolumePercent);
  }
}

void WizardController::AutoLaunchKioskApp() {
  KioskAppManager::App app_data;
  std::string app_id = KioskAppManager::Get()->GetAutoLaunchApp();
  CHECK(KioskAppManager::Get()->GetApp(app_id, &app_data));

  host_->StartAppLaunch(app_id, false /* diagnostic_mode */);
}

// static
void WizardController::SetZeroDelays() {
  kShowDelayMs = 0;
  zero_delay_enabled_ = true;
}

// static
bool WizardController::IsZeroDelayEnabled() {
  return zero_delay_enabled_;
}

// static
bool WizardController::IsOOBEStepToTrack(const std::string& screen_id) {
  return (screen_id == kHIDDetectionScreenName ||
          screen_id == kNetworkScreenName ||
          screen_id == kUpdateScreenName ||
          screen_id == kUserImageScreenName ||
          screen_id == kEulaScreenName ||
          screen_id == kLoginScreenName ||
          screen_id == kWrongHWIDScreenName);
}

// static
void WizardController::SkipPostLoginScreensForTesting() {
  skip_post_login_screens_ = true;
}

// static
bool WizardController::ShouldAutoStartEnrollment() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyInitializer* dcp_initializer =
      connector->GetDeviceCloudPolicyInitializer();
  return dcp_initializer && dcp_initializer->ShouldAutoStartEnrollment();
}

// static
bool WizardController::ShouldRecoverEnrollment() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyInitializer* dcp_initializer =
      connector->GetDeviceCloudPolicyInitializer();
  return dcp_initializer && dcp_initializer->ShouldRecoverEnrollment();
}

// static
std::string WizardController::GetEnrollmentRecoveryDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyInitializer* dcp_initializer =
      connector->GetDeviceCloudPolicyInitializer();
  if (!dcp_initializer)
    return std::string();
  return dcp_initializer->GetEnrollmentRecoveryDomain();
}

// static
bool WizardController::CanExitEnrollment() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  CHECK(connector);
  return connector->GetDeviceCloudPolicyInitializer()->CanExitEnrollment();
}

// static
std::string WizardController::GetForcedEnrollmentDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  CHECK(connector);
  return connector->GetDeviceCloudPolicyInitializer()
      ->GetForcedEnrollmentDomain();
}

void WizardController::OnLocalStateInitialized(bool /* succeeded */) {
  if (GetLocalState()->GetInitializationStatus() !=
      PrefService::INITIALIZATION_STATUS_ERROR) {
    return;
  }
  GetErrorScreen()->SetUIState(ErrorScreen::UI_STATE_LOCAL_STATE_ERROR);
  SetStatusAreaVisible(false);
  ShowErrorScreen();
}

PrefService* WizardController::GetLocalState() {
  if (local_state_for_testing_)
    return local_state_for_testing_;
  return g_browser_process->local_state();
}

void WizardController::OnTimezoneResolved(
    scoped_ptr<TimeZoneResponseData> timezone,
    bool server_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(timezone.get());
  // To check that "this" is not destroyed try to access some member
  // (timezone_provider_) in this case. Expect crash here.
  DCHECK(timezone_provider_.get());

  timezone_resolved_ = true;
  base::ScopedClosureRunner inform_test(on_timezone_resolved_for_testing_);
  on_timezone_resolved_for_testing_.Reset();

  VLOG(1) << "Resolved local timezone={" << timezone->ToStringForDebug()
          << "}.";

  if (timezone->status != TimeZoneResponseData::OK) {
    LOG(WARNING) << "Resolve TimeZone: failed to resolve timezone.";
    return;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged()) {
    std::string policy_timezone;
    if (chromeos::CrosSettings::Get()->GetString(
            chromeos::kSystemTimezonePolicy, &policy_timezone) &&
        !policy_timezone.empty()) {
      VLOG(1) << "Resolve TimeZone: TimeZone settings are overridden"
              << " by DevicePolicy.";
      return;
    }
  }

  if (!timezone->timeZoneId.empty()) {
    VLOG(1) << "Resolve TimeZone: setting timezone to '" << timezone->timeZoneId
            << "'";

    chromeos::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
        base::UTF8ToUTF16(timezone->timeZoneId));
  }
}

TimeZoneProvider* WizardController::GetTimezoneProvider() {
  if (!timezone_provider_) {
    timezone_provider_.reset(
        new TimeZoneProvider(g_browser_process->system_request_context(),
                             DefaultTimezoneProviderURL()));
  }
  return timezone_provider_.get();
}

void WizardController::OnLocationResolved(const Geoposition& position,
                                          bool server_error,
                                          const base::TimeDelta elapsed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::TimeDelta timeout =
      base::TimeDelta::FromSeconds(kResolveTimeZoneTimeoutSeconds);
  // Ignore invalid position.
  if (!position.Valid())
    return;

  if (elapsed >= timeout) {
    LOG(WARNING) << "Resolve TimeZone: got location after timeout ("
                 << elapsed.InSecondsF() << " seconds elapsed). Ignored.";
    return;
  }

  // WizardController owns TimezoneProvider, so timezone request is silently
  // cancelled on destruction.
  GetTimezoneProvider()->RequestTimezone(
      position,
      false,  // sensor
      timeout - elapsed,
      base::Bind(&WizardController::OnTimezoneResolved,
                 base::Unretained(this)));
}

bool WizardController::SetOnTimeZoneResolvedForTesting(
    const base::Closure& callback) {
  if (timezone_resolved_)
    return false;

  on_timezone_resolved_for_testing_ = callback;
  return true;
}

}  // namespace chromeos
