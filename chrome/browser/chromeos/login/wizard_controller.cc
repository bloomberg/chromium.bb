// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_view.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/screens/network_view.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/timezone/timezone_provider.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/pairing/bluetooth_controller_pairing_controller.h"
#include "components/pairing/bluetooth_host_pairing_controller.h"
#include "components/pairing/shark_connection_listener.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/accelerators/accelerator.h"

using content::BrowserThread;

namespace {
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
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableHIDDetectionOnOOBE);
}

bool IsResumableScreen(const std::string& screen) {
  for (size_t i = 0; i < arraysize(kResumableScreens); ++i) {
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

bool IsRemoraRequisition() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetDeviceCloudPolicyManager()
      ->IsRemoraRequisition();
}

// Checks if the device is a "Slave" device in the bootstrapping process.
bool IsBootstrappingSlave() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kOobeBootstrappingSlave);
}

// Checks if the device is a "Master" device in the bootstrapping process.
bool IsBootstrappingMaster() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kOobeBootstrappingMaster);
}

bool NetworkAllowUpdate(const chromeos::NetworkState* network) {
  if (!network || !network->IsConnectedState())
    return false;
  if (network->type() == shill::kTypeBluetooth ||
      (network->type() == shill::kTypeCellular &&
       !help_utils_chromeos::IsUpdateOverCellularAllowed())) {
    return false;
  }
  return true;
}

#if defined(GOOGLE_CHROME_BUILD)
void InitializeCrashReporter() {
  // The crash reporter initialization needs IO to complete.
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  breakpad::InitCrashReporter(std::string());
}
#endif

}  // namespace

namespace chromeos {

const char WizardController::kNetworkScreenName[] = "network";
const char WizardController::kLoginScreenName[] = "login";
const char WizardController::kUpdateScreenName[] = "update";
const char WizardController::kUserImageScreenName[] = "image";
const char WizardController::kEulaScreenName[] = "eula";
const char WizardController::kEnableDebuggingScreenName[] = "debugging";
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
const char WizardController::kDeviceDisabledScreenName[] = "device-disabled";

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

WizardController::WizardController(LoginDisplayHost* host,
                                   OobeDisplay* oobe_display)
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
      retry_auto_enrollment_check_(false),
      login_screen_started_(false),
      user_image_screen_return_to_previous_hack_(false),
      timezone_resolved_(false),
      shark_controller_detected_(false),
      hid_screen_(nullptr),
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

void WizardController::Init(const std::string& first_screen_name) {
  VLOG(1) << "Starting OOBE wizard with screen: " << first_screen_name;
  first_screen_name_ = first_screen_name;

  bool oobe_complete = StartupUtils::IsOobeCompleted();
  if (!oobe_complete || first_screen_name == kOutOfBoxScreenName)
    is_out_of_box_ = true;

  // This is a hacky way to check for local state corruption, because
  // it depends on the fact that the local state is loaded
  // synchronously and at the first demand. IsEnterpriseManaged()
  // check is required because currently powerwash is disabled for
  // enterprise-enrolled devices.
  //
  // TODO (ygorshenin@): implement handling of the local state
  // corruption in the case of asynchronious loading.
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

  // If the device is a Master device in bootstrapping process (mostly for demo
  // and test purpose), start the enrollment OOBE flow.
  if (IsBootstrappingMaster())
    connector->GetDeviceCloudPolicyManager()->SetDeviceEnrollmentAutoStart();

  // Use the saved screen preference from Local State.
  const std::string screen_pref =
      GetLocalState()->GetString(prefs::kOobeScreenPending);
  if (is_out_of_box_ && !screen_pref.empty() && !IsHostPairingOobe() &&
      (first_screen_name.empty() ||
       first_screen_name == WizardController::kTestNoScreenName)) {
    first_screen_name_ = screen_pref;
  }

  AdvanceToScreen(first_screen_name_);
  if (!IsMachineHWIDCorrect() && !StartupUtils::IsDeviceRegistered() &&
      first_screen_name_.empty())
    ShowWrongHWIDScreen();
}

ErrorScreen* WizardController::GetErrorScreen() {
  return oobe_display_->GetErrorScreen();
}

BaseScreen* WizardController::GetScreen(const std::string& screen_name) {
  if (screen_name == kErrorScreenName)
    return GetErrorScreen();
  return ScreenManager::GetScreen(screen_name);
}

BaseScreen* WizardController::CreateScreen(const std::string& screen_name) {
  if (screen_name == kNetworkScreenName) {
    scoped_ptr<NetworkScreen> screen(
        new NetworkScreen(this, this, oobe_display_->GetNetworkView()));
    screen->Initialize(nullptr /* context */);
    return screen.release();
  } else if (screen_name == kUpdateScreenName) {
    scoped_ptr<UpdateScreen> screen(new UpdateScreen(
        this, oobe_display_->GetUpdateView(), remora_controller_.get()));
    screen->Initialize(nullptr /* context */);
    return screen.release();
  } else if (screen_name == kUserImageScreenName) {
    return new UserImageScreen(this, oobe_display_->GetUserImageView());
  } else if (screen_name == kEulaScreenName) {
    return new EulaScreen(this, this, oobe_display_->GetEulaView());
  } else if (screen_name == kEnrollmentScreenName) {
    return new EnrollmentScreen(this,
                                oobe_display_->GetEnrollmentScreenActor());
  } else if (screen_name == kResetScreenName) {
    return new chromeos::ResetScreen(this,
                                     oobe_display_->GetResetView());
  } else if (screen_name == kEnableDebuggingScreenName) {
    return new EnableDebuggingScreen(
        this, oobe_display_->GetEnableDebuggingScreenActor());
  } else if (screen_name == kKioskEnableScreenName) {
    return new KioskEnableScreen(this,
                                 oobe_display_->GetKioskEnableScreenActor());
  } else if (screen_name == kKioskAutolaunchScreenName) {
    return new KioskAutolaunchScreen(
        this, oobe_display_->GetKioskAutolaunchScreenActor());
  } else if (screen_name == kTermsOfServiceScreenName) {
    return new TermsOfServiceScreen(
        this, oobe_display_->GetTermsOfServiceScreenActor());
  } else if (screen_name == kWrongHWIDScreenName) {
    return new WrongHWIDScreen(this, oobe_display_->GetWrongHWIDScreenActor());
  } else if (screen_name == kSupervisedUserCreationScreenName) {
    return new SupervisedUserCreationScreen(
        this, oobe_display_->GetSupervisedUserCreationScreenActor());
  } else if (screen_name == kHIDDetectionScreenName) {
    scoped_ptr<HIDDetectionScreen> screen(new chromeos::HIDDetectionScreen(
        this, oobe_display_->GetHIDDetectionView()));
    screen->Initialize(nullptr /* context */);
    return screen.release();
  } else if (screen_name == kAutoEnrollmentCheckScreenName) {
    return new AutoEnrollmentCheckScreen(
        this, oobe_display_->GetAutoEnrollmentCheckScreenActor());
  } else if (screen_name == kControllerPairingScreenName) {
    if (!shark_controller_) {
      shark_controller_.reset(
          new pairing_chromeos::BluetoothControllerPairingController());
    }
    return new ControllerPairingScreen(
        this,
        this,
        oobe_display_->GetControllerPairingScreenActor(),
        shark_controller_.get());
  } else if (screen_name == kHostPairingScreenName) {
    if (!remora_controller_) {
      remora_controller_.reset(
          new pairing_chromeos::BluetoothHostPairingController());
      remora_controller_->StartPairing();
    }
    return new HostPairingScreen(this,
                                 this,
                                 oobe_display_->GetHostPairingScreenActor(),
                                 remora_controller_.get());
  } else if (screen_name == kDeviceDisabledScreenName) {
    return new DeviceDisabledScreen(
        this, oobe_display_->GetDeviceDisabledScreenActor());
  }

  return NULL;
}

void WizardController::ShowNetworkScreen() {
  VLOG(1) << "Showing network screen.";
  // Hide the status area initially; it only appears after OOBE first animates
  // in. Keep it visible if the user goes back to the existing network screen.
  SetStatusAreaVisible(HasScreen(kNetworkScreenName));
  SetCurrentScreen(GetScreen(kNetworkScreenName));

  MaybeStartListeningForSharkConnection();
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

void WizardController::ShowUpdateScreen() {
  VLOG(1) << "Showing update screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetScreen(kUpdateScreenName));
}

void WizardController::ShowUserImageScreen() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  // Skip user image selection for public sessions and ephemeral non-regual user
  // logins.
  if (user_manager->IsLoggedInAsPublicAccount() ||
      (user_manager->IsCurrentUserNonCryptohomeDataEphemeral() &&
       user_manager->GetLoggedInUser()->GetType() !=
           user_manager::USER_TYPE_REGULAR)) {
    OnUserImageSkipped();
    return;
  }
  VLOG(1) << "Showing user image screen.";

  // Status area has been already shown at sign in screen so it
  // doesn't make sense to hide it here and then show again at user session as
  // this produces undesired UX transitions.
  SetStatusAreaVisible(true);

  SetCurrentScreen(GetScreen(kUserImageScreenName));
}

void WizardController::ShowEulaScreen() {
  VLOG(1) << "Showing EULA screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetScreen(kEulaScreenName));
}

void WizardController::ShowEnrollmentScreen() {
  // Update the enrollment configuration and start the screen.
  prescribed_enrollment_config_ = g_browser_process->platform_part()
                                      ->browser_policy_connector_chromeos()
                                      ->GetPrescribedEnrollmentConfig();
  StartEnrollmentScreen();
}

void WizardController::ShowResetScreen() {
  VLOG(1) << "Showing reset screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetScreen(kResetScreenName));
}

void WizardController::ShowKioskEnableScreen() {
  VLOG(1) << "Showing kiosk enable screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetScreen(kKioskEnableScreenName));
}

void WizardController::ShowKioskAutolaunchScreen() {
  VLOG(1) << "Showing kiosk autolaunch screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetScreen(kKioskAutolaunchScreenName));
}

void WizardController::ShowEnableDebuggingScreen() {
  VLOG(1) << "Showing enable developer features screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetScreen(kEnableDebuggingScreenName));
}

void WizardController::ShowTermsOfServiceScreen() {
  // Only show the Terms of Service when logging into a public account and Terms
  // of Service have been specified through policy. In all other cases, advance
  // to the user image screen immediately.
  if (!user_manager::UserManager::Get()->IsLoggedInAsPublicAccount() ||
      !ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          prefs::kTermsOfServiceURL)) {
    ShowUserImageScreen();
    return;
  }

  VLOG(1) << "Showing Terms of Service screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetScreen(kTermsOfServiceScreenName));
}

void WizardController::ShowWrongHWIDScreen() {
  VLOG(1) << "Showing wrong HWID screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetScreen(kWrongHWIDScreenName));
}

void WizardController::ShowAutoEnrollmentCheckScreen() {
  VLOG(1) << "Showing Auto-enrollment check screen.";
  SetStatusAreaVisible(true);
  AutoEnrollmentCheckScreen* screen = AutoEnrollmentCheckScreen::Get(this);
  if (retry_auto_enrollment_check_)
    screen->ClearState();
  screen->set_auto_enrollment_controller(host_->GetAutoEnrollmentController());
  SetCurrentScreen(screen);
}

void WizardController::ShowSupervisedUserCreationScreen() {
  VLOG(1) << "Showing Locally managed user creation screen screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetScreen(kSupervisedUserCreationScreenName));
}

void WizardController::ShowHIDDetectionScreen() {
  VLOG(1) << "Showing HID discovery screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetScreen(kHIDDetectionScreenName));
  MaybeStartListeningForSharkConnection();
}

void WizardController::ShowControllerPairingScreen() {
  VLOG(1) << "Showing controller pairing screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetScreen(kControllerPairingScreenName));
}

void WizardController::ShowHostPairingScreen() {
  VLOG(1) << "Showing host pairing screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetScreen(kHostPairingScreenName));
}

void WizardController::ShowDeviceDisabledScreen() {
  VLOG(1) << "Showing device disabled screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetScreen(kDeviceDisabledScreenName));
}

void WizardController::SkipToLoginForTesting(
    const LoginScreenContext& context) {
  VLOG(1) << "SkipToLoginForTesting.";
  StartupUtils::MarkEulaAccepted();
  PerformPostEulaActions();
  OnDeviceDisabledChecked(false /* device_disabled */);
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
  const bool is_shark = g_browser_process->platform_part()
                            ->browser_policy_connector_chromeos()
                            ->GetDeviceCloudPolicyManager()
                            ->IsSharkRequisition();
  if (is_shark || IsBootstrappingMaster()) {
    ShowControllerPairingScreen();
  } else if (IsBootstrappingSlave() && shark_controller_detected_) {
    ShowHostPairingScreen();
  } else {
    ShowAutoEnrollmentCheckScreen();
  }
}

void WizardController::OnEulaAccepted() {
  time_eula_accepted_ = base::Time::Now();
  StartupUtils::MarkEulaAccepted();
  InitiateMetricsReportingChange(
      usage_statistics_reporting_,
      base::Bind(&WizardController::InitiateMetricsReportingChangeCallback,
                 weak_factory_.GetWeakPtr()));

  if (skip_update_enroll_after_eula_) {
    PerformPostEulaActions();
    ShowAutoEnrollmentCheckScreen();
  } else {
    InitiateOOBEUpdate();
  }
}

void WizardController::InitiateMetricsReportingChangeCallback(bool enabled) {
  CrosSettings::Get()->SetBoolean(kStatsReportingPref, enabled);
  if (!enabled)
    return;
#if defined(GOOGLE_CHROME_BUILD)
  if (!content::BrowserThread::PostBlockingPoolTask(
          FROM_HERE, base::Bind(&InitializeCrashReporter))) {
    LOG(ERROR) << "Failed to start crash reporter initialization.";
  }
#endif
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
      BrowserThread::UI, FROM_HERE,
      base::Bind(&UserSessionManager::DoBrowserLaunch,
                 base::Unretained(UserSessionManager::GetInstance()),
                 ProfileManager::GetActiveUserProfile(), host_));
  host_ = NULL;
}

void WizardController::OnUserImageSkipped() {
  OnUserImageSelected();
}

void WizardController::OnEnrollmentDone() {
  // If the enrollment screen was shown as part of OOBE, OOBE is considered
  // finished only after the enrollment screen is done. This is relevant for
  // forced enrollment flows, e.g. for remora devices and forced re-enrollment.
  if (prescribed_enrollment_config_.should_enroll())
    PerformOOBECompletedActions();

  // TODO(mnissler): Unify the logic for auto-login for Public Sessions and
  // Kiosk Apps and make this code cover both cases: http://crbug.com/234694.
  if (KioskAppManager::Get()->IsAutoLaunchEnabled())
    AutoLaunchKioskApp();
  else
    ShowLoginScreen(LoginScreenContext());
}

void WizardController::OnDeviceModificationCanceled() {
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

void WizardController::OnAutoEnrollmentCheckCompleted() {
  // Check whether the device is disabled. OnDeviceDisabledChecked() will be
  // invoked when the result of this check is known. Until then, the current
  // screen will remain visible and will continue showing a spinner.
  g_browser_process->platform_part()->device_disabling_manager()->
      CheckWhetherDeviceDisabledDuringOOBE(base::Bind(
          &WizardController::OnDeviceDisabledChecked,
          weak_factory_.GetWeakPtr()));
}

void WizardController::OnDeviceDisabledChecked(bool device_disabled) {
  prescribed_enrollment_config_ = g_browser_process->platform_part()
                                      ->browser_policy_connector_chromeos()
                                      ->GetPrescribedEnrollmentConfig();
  if (device_disabled) {
    ShowDeviceDisabledScreen();
  } else if (skip_update_enroll_after_eula_ ||
             prescribed_enrollment_config_.should_enroll()) {
    StartEnrollmentScreen();
  } else {
    PerformOOBECompletedActions();
    ShowLoginScreen(LoginScreenContext());
  }
}

void WizardController::InitiateOOBEUpdate() {
  VLOG(1) << "InitiateOOBEUpdate";
  PerformPostEulaActions();
  SetCurrentScreenSmooth(GetScreen(kUpdateScreenName), true);
  UpdateScreen::Get(this)->StartNetworkCheck();
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
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
      base::Bind(&WizardController::StartTimezoneResolve,
                 weak_factory_.GetWeakPtr()));
  DelayNetworkCall(
      base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
      ServicesCustomizationDocument::GetInstance()
          ->EnsureCustomizationAppliedClosure());

  // Now that EULA has been accepted (for official builds), enable portal check.
  // ChromiumOS builds would go though this code path too.
  NetworkHandler::Get()->network_state_handler()->SetCheckPortalList(
      NetworkStateHandler::kDefaultCheckPortalList);
  host_->GetAutoEnrollmentController()->Start();
  host_->PrewarmAuthentication();
  network_portal_detector::GetInstance()->Enable(true);
}

void WizardController::PerformOOBECompletedActions() {
  UMA_HISTOGRAM_COUNTS_100(
      "HIDDetection.TimesDialogShownPerOOBECompleted",
      GetLocalState()->GetInteger(prefs::kTimesHIDDialogShown));
  GetLocalState()->ClearPref(prefs::kTimesHIDDialogShown);
  StartupUtils::MarkOobeCompleted();

  // Restart to make the login page pick up the policy changes resulting from
  // enrollment recovery.
  // TODO(tnagel): Find a way to update login page without reboot.
  if (prescribed_enrollment_config_.mode ==
      policy::EnrollmentConfig::MODE_RECOVERY) {
    chrome::AttemptRestart();
  }
}

void WizardController::SetCurrentScreen(BaseScreen* new_current) {
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

void WizardController::SetCurrentScreenSmooth(BaseScreen* new_current,
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
  } else if (screen_name == kEnableDebuggingScreenName) {
    ShowEnableDebuggingScreen();
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
  } else if (screen_name == kDeviceDisabledScreenName) {
    ShowDeviceDisabledScreen();
  } else if (screen_name != kTestNoScreenName) {
    if (is_out_of_box_) {
      time_oobe_started_ = base::Time::Now();
      if (IsHostPairingOobe()) {
        ShowHostPairingScreen();
      } else if (CanShowHIDDetectionScreen()) {
        hid_screen_ = GetScreen(kHIDDetectionScreenName);
        base::Callback<void(bool)> on_check = base::Bind(
            &WizardController::OnHIDScreenNecessityCheck,
            weak_factory_.GetWeakPtr());
        oobe_display_->GetHIDDetectionView()->CheckIsScreenRequired(on_check);
      } else {
        ShowNetworkScreen();
      }
    } else {
      ShowLoginScreen(LoginScreenContext());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, BaseScreenDelegate overrides:
void WizardController::OnExit(BaseScreen& /* screen */,
                              ExitCodes exit_code,
                              const ::login::ScreenContext* /* context */) {
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
    case ENABLE_DEBUGGING_CANCELED:
      OnDeviceModificationCanceled();
      break;
    case ENABLE_DEBUGGING_FINISHED:
      OnDeviceModificationCanceled();
      break;
    case ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED:
      OnAutoEnrollmentCheckCompleted();
      break;
    case ENTERPRISE_ENROLLMENT_COMPLETED:
      OnEnrollmentDone();
      break;
    case ENTERPRISE_ENROLLMENT_BACK:
      retry_auto_enrollment_check_ = true;
      ShowAutoEnrollmentCheckScreen();
      break;
    case RESET_CANCELED:
      OnDeviceModificationCanceled();
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
    default:
      NOTREACHED();
  }
}

void WizardController::ShowErrorScreen() {
  VLOG(1) << "Showing error screen.";
  SetCurrentScreen(GetScreen(kErrorScreenName));
}

void WizardController::HideErrorScreen(BaseScreen* parent_screen) {
  DCHECK(parent_screen);
  VLOG(1) << "Hiding error screen.";
  SetCurrentScreen(parent_screen);
}

void WizardController::SetUsageStatisticsReporting(bool val) {
  usage_statistics_reporting_ = val;
}

bool WizardController::GetUsageStatisticsReporting() const {
  return usage_statistics_reporting_;
}

void WizardController::SetHostNetwork() {
  if (!shark_controller_)
    return;
  NetworkScreen* network_screen = NetworkScreen::Get(this);
  std::string onc_spec;
  network_screen->GetConnectedWifiNetwork(&onc_spec);
  if (!onc_spec.empty())
    shark_controller_->SetHostNetwork(onc_spec);
}

void WizardController::SetHostConfiguration() {
  if (!shark_controller_)
    return;
  NetworkScreen* network_screen = NetworkScreen::Get(this);
  shark_controller_->SetHostConfiguration(
      true,  // Eula must be accepted before we get this far.
      network_screen->GetApplicationLocale(), network_screen->GetTimezone(),
      GetUsageStatisticsReporting(), network_screen->GetInputMethod());
}

void WizardController::ConfigureHostRequested(
    bool accepted_eula,
    const std::string& lang,
    const std::string& timezone,
    bool send_reports,
    const std::string& keyboard_layout) {
  VLOG(1) << "ConfigureHost locale=" << lang << ", timezone=" << timezone
          << ", keyboard_layout=" << keyboard_layout;
  if (accepted_eula)  // Always true.
    StartupUtils::MarkEulaAccepted();
  SetUsageStatisticsReporting(send_reports);

  NetworkScreen* network_screen = NetworkScreen::Get(this);
  network_screen->SetApplicationLocaleAndInputMethod(lang, keyboard_layout);
  network_screen->SetTimezone(timezone);

  // Don't block the OOBE update and the following enrollment process if there
  // is available and valid network already.
  const chromeos::NetworkState* network_state = chromeos::NetworkHandler::Get()
                                                    ->network_state_handler()
                                                    ->DefaultNetwork();
  if (NetworkAllowUpdate(network_state))
    InitiateOOBEUpdate();
}

void WizardController::AddNetworkRequested(const std::string& onc_spec) {
  NetworkScreen* network_screen = NetworkScreen::Get(this);
  const chromeos::NetworkState* network_state = chromeos::NetworkHandler::Get()
                                                    ->network_state_handler()
                                                    ->DefaultNetwork();

  if (NetworkAllowUpdate(network_state)) {
    network_screen->CreateAndConnectNetworkFromOnc(
        onc_spec, base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));
  } else {
    network_screen->CreateAndConnectNetworkFromOnc(
        onc_spec, base::Bind(&WizardController::InitiateOOBEUpdate,
                             weak_factory_.GetWeakPtr()),
        base::Bind(&WizardController::OnSetHostNetworkFailed,
                   weak_factory_.GetWeakPtr()));
  }
}

void WizardController::OnEnableDebuggingScreenRequested() {
  if (!login_screen_started())
    AdvanceToScreen(WizardController::kEnableDebuggingScreenName);
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

  // Wait for the |CrosSettings| to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(base::Bind(
          &WizardController::AutoLaunchKioskApp,
          weak_factory_.GetWeakPtr()));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the |cros_settings_| are permanently untrusted, show an error message
    // and refuse to auto-launch the kiosk app.
    GetErrorScreen()->SetUIState(NetworkError::UI_STATE_LOCAL_STATE_ERROR);
    SetStatusAreaVisible(false);
    ShowErrorScreen();
    return;
  }

  bool device_disabled = false;
  CrosSettings::Get()->GetBoolean(kDeviceDisabled, &device_disabled);
  if (device_disabled && system::DeviceDisablingManager::
                             HonorDeviceDisablingDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.
    return;
  }

  const bool diagnostic_mode = false;
  const bool auto_launch = true;
  host_->StartAppLaunch(app_id, diagnostic_mode, auto_launch);
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

void WizardController::OnLocalStateInitialized(bool /* succeeded */) {
  if (GetLocalState()->GetInitializationStatus() !=
      PrefService::INITIALIZATION_STATUS_ERROR) {
    return;
  }
  GetErrorScreen()->SetUIState(NetworkError::UI_STATE_LOCAL_STATE_ERROR);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    if (CrosSettings::Get()->GetString(kSystemTimezonePolicy,
                                       &policy_timezone) &&
        !policy_timezone.empty()) {
      VLOG(1) << "Resolve TimeZone: TimeZone settings are overridden"
              << " by DevicePolicy.";
      return;
    }
  }

  if (!timezone->timeZoneId.empty()) {
    VLOG(1) << "Resolve TimeZone: setting timezone to '" << timezone->timeZoneId
            << "'";

    system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

bool WizardController::IsHostPairingOobe() const {
  return (IsRemoraRequisition() || IsBootstrappingSlave()) &&
         (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kHostPairingOobe) ||
          shark_controller_detected_);
}

void WizardController::MaybeStartListeningForSharkConnection() {
  if (!IsRemoraRequisition() && !IsBootstrappingSlave())
    return;

  // We shouldn't be here if we are running pairing OOBE already.
  DCHECK(!IsHostPairingOobe());

  if (!shark_connection_listener_) {
    shark_connection_listener_.reset(
        new pairing_chromeos::SharkConnectionListener(
            base::Bind(&WizardController::OnSharkConnected,
                       weak_factory_.GetWeakPtr())));
  }
}

void WizardController::OnSharkConnected(
    scoped_ptr<pairing_chromeos::HostPairingController> remora_controller) {
  VLOG(1) << "OnSharkConnected";
  remora_controller_ = std::move(remora_controller);
  base::MessageLoop::current()->DeleteSoon(
      FROM_HERE, shark_connection_listener_.release());
  shark_controller_detected_ = true;
  ShowHostPairingScreen();
}

void WizardController::OnSetHostNetworkFailed() {
  remora_controller_->OnNetworkConnectivityChanged(
      pairing_chromeos::HostPairingController::CONNECTIVITY_NONE);
}

void WizardController::StartEnrollmentScreen() {
  VLOG(1) << "Showing enrollment screen.";

  // Determine the effective enrollment configuration. If there is a valid
  // prescribed configuration, use that. If not, figure out which variant of
  // manual enrollment is taking place.
  policy::EnrollmentConfig effective_config = prescribed_enrollment_config_;
  if (!effective_config.should_enroll()) {
    effective_config.mode =
        prescribed_enrollment_config_.management_domain.empty()
            ? policy::EnrollmentConfig::MODE_MANUAL
            : policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT;
  }

  EnrollmentScreen* screen = EnrollmentScreen::Get(this);
  screen->SetParameters(effective_config, shark_controller_.get());
  SetStatusAreaVisible(true);
  SetCurrentScreen(screen);
}

}  // namespace chromeos
