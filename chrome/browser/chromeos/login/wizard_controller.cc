// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <signal.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/eula_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/html_page_screen.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/chromeos/login/registration_screen.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"
#include "chrome/browser/chromeos/login/update_screen.h"
#include "chrome/browser/chromeos/login/user_image_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

using content::BrowserThread;

namespace {

// A boolean pref of the EULA accepted flag.
const char kEulaAccepted[] = "EulaAccepted";

// A string pref with initial locale set in VPD or manifest.
const char kInitialLocale[] = "intl.initial_locale";

// A boolean pref of the OOBE complete flag (first OOBE part before login).
const char kOobeComplete[] = "OobeComplete";

// A boolean pref of the device registered flag (second part after first login).
const char kDeviceRegistered[] = "DeviceRegistered";

// Path to flag file indicating that both parts of OOBE were completed.
const char kOobeCompleteFlagFilePath[] = "/home/chronos/.oobe_completed";

// Time in seconds that we wait for the device to reboot.
// If reboot didn't happen, ask user to reboot device manually.
const int kWaitForRebootTimeSec = 3;

// Interval in ms which is used for smooth screen showing.
static int kShowDelayMs = 400;

// Saves boolean "Local State" preference and forces its persistence to disk.
void SaveBoolPreferenceForced(const char* pref_name, bool value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(pref_name, value);
  prefs->SavePersistentPrefs();
}

// Saves integer "Local State" preference and forces its persistence to disk.
void SaveIntegerPreferenceForced(const char* pref_name, int value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(pref_name, value);
  prefs->SavePersistentPrefs();
}

// Saves string "Local State" preference and forces its persistence to disk.
void SaveStringPreferenceForced(const char* pref_name,
                                const std::string& value) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(pref_name, value);
  prefs->SavePersistentPrefs();
}

}  // namespace

namespace chromeos {

const char WizardController::kNetworkScreenName[] = "network";
const char WizardController::kLoginScreenName[] = "login";
const char WizardController::kUpdateScreenName[] = "update";
const char WizardController::kUserImageScreenName[] = "image";
const char WizardController::kEulaScreenName[] = "eula";
const char WizardController::kRegistrationScreenName[] = "register";
const char WizardController::kHTMLPageScreenName[] = "html";
const char WizardController::kEnterpriseEnrollmentScreenName[] = "enroll";

// Passing this parameter as a "first screen" initiates full OOBE flow.
const char WizardController::kOutOfBoxScreenName[] = "oobe";

// Special test value that commands not to create any window yet.
const char WizardController::kTestNoScreenName[] = "test:nowindow";

// Initialize default controller.
// static
WizardController* WizardController::default_controller_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:

WizardController::WizardController(chromeos::LoginDisplayHost* host,
                                   chromeos::OobeDisplay* oobe_display)
    : current_screen_(NULL),
#if defined(OFFICIAL_BUILD)
      is_official_build_(true),
#else
      is_official_build_(false),
#endif
      is_out_of_box_(false),
      host_(host),
      oobe_display_(oobe_display),
      usage_statistics_reporting_(true) {
  DCHECK(default_controller_ == NULL);
  default_controller_ = this;
}

WizardController::~WizardController() {
  if (default_controller_ == this) {
    default_controller_ = NULL;
  } else {
    NOTREACHED() << "More than one controller are alive.";
  }

  chromeos::WizardAccessibilityHelper::GetInstance()->
      UnregisterNotifications();
}

void WizardController::Init(const std::string& first_screen_name) {
  VLOG(1) << "Starting OOBE wizard with screen: " << first_screen_name;
  first_screen_name_ = first_screen_name;

  bool oobe_complete = IsOobeCompleted();
  if (!oobe_complete || first_screen_name == kOutOfBoxScreenName) {
    is_out_of_box_ = true;
  }

  ShowFirstScreen(first_screen_name);
}

void WizardController::CancelOOBEUpdate() {
  if (update_screen_.get() &&
      update_screen_.get() == current_screen_) {
    GetUpdateScreen()->CancelUpdate();
  }
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

chromeos::RegistrationScreen* WizardController::GetRegistrationScreen() {
  if (!registration_screen_.get())
    registration_screen_.reset(
        new chromeos::RegistrationScreen(
            oobe_display_->GetRegistrationScreenActor()));
  return registration_screen_.get();
}

chromeos::HTMLPageScreen* WizardController::GetHTMLPageScreen() {
  if (!html_page_screen_.get()) {
    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
    const CommandLine::StringVector& args = cmd_line->GetArgs();

    std::string url;
    // It's strange but args may contains empty strings.
    for (size_t i = 0; i < args.size(); i++) {
      if (!args[i].empty()) {
        DCHECK(url.empty()) << "More than one URL in command line";
        url = args[i];
      }
    }
    DCHECK(!url.empty()) << "No URL in command line";
    html_page_screen_.reset(
        new chromeos::HTMLPageScreen(
            oobe_display_->GetHTMLPageScreenActor(), url));
  }
  return html_page_screen_.get();
}

chromeos::EnterpriseEnrollmentScreen*
    WizardController::GetEnterpriseEnrollmentScreen() {
  if (!enterprise_enrollment_screen_.get()) {
    enterprise_enrollment_screen_.reset(
        new chromeos::EnterpriseEnrollmentScreen(
            this, oobe_display_->GetEnterpriseEnrollmentScreenActor()));
  }
  return enterprise_enrollment_screen_.get();
}

void WizardController::ShowNetworkScreen() {
  VLOG(1) << "Showing network screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetNetworkScreen());
  host_->SetOobeProgress(chromeos::BackgroundView::SELECT_NETWORK);
}

void WizardController::ShowLoginScreen() {
  VLOG(1) << "Showing login screen.";
  SetStatusAreaVisible(true);
  host_->SetOobeProgress(chromeos::BackgroundView::SIGNIN);
  host_->StartSignInScreen();
  smooth_show_timer_.Stop();
  oobe_display_ = NULL;
}

void WizardController::ShowUpdateScreen() {
  VLOG(1) << "Showing update screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetUpdateScreen());
  // There is no special step for update.
#if defined(OFFICIAL_BUILD)
  host_->SetOobeProgress(chromeos::BackgroundView::EULA);
#else
  host_->SetOobeProgress(chromeos::BackgroundView::SELECT_NETWORK);
#endif
}

void WizardController::ShowUserImageScreen() {
  VLOG(1) << "Showing user image screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetUserImageScreen());
  host_->SetOobeProgress(chromeos::BackgroundView::PICTURE);
  host_->SetShutdownButtonEnabled(false);
}

void WizardController::ShowEulaScreen() {
  VLOG(1) << "Showing EULA screen.";
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetEulaScreen());
#if defined(OFFICIAL_BUILD)
  host_->SetOobeProgress(chromeos::BackgroundView::EULA);
#endif
}

void WizardController::ShowRegistrationScreen() {
  if (!IsRegisterScreenDefined()) {
    VLOG(1) << "Skipping registration screen: manifest not defined or invalid "
               "URL.";
    OnRegistrationSkipped();
    return;
  }
  VLOG(1) << "Showing registration screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetRegistrationScreen());
#if defined(OFFICIAL_BUILD)
  host_->SetOobeProgress(chromeos::BackgroundView::REGISTRATION);
#endif
}

void WizardController::ShowHTMLPageScreen() {
  VLOG(1) << "Showing HTML page screen.";
  SetStatusAreaVisible(true);
  host_->SetOobeProgressBarVisible(false);
  SetCurrentScreen(GetHTMLPageScreen());
}

void WizardController::ShowEnterpriseEnrollmentScreen() {
  SetStatusAreaVisible(true);
  host_->SetOobeProgress(chromeos::BackgroundView::SIGNIN);
  SetCurrentScreen(GetEnterpriseEnrollmentScreen());
}

void WizardController::SkipRegistration() {
  if (current_screen_ == GetRegistrationScreen())
    OnRegistrationSkipped();
  else
    LOG(ERROR) << "Registration screen is not active.";
}

// static
void WizardController::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterBooleanPref(kOobeComplete,
                                   false,
                                   PrefService::UNSYNCABLE_PREF);
  local_state->RegisterIntegerPref(kDeviceRegistered,
                                   -1,
                                   PrefService::UNSYNCABLE_PREF);
  local_state->RegisterBooleanPref(kEulaAccepted,
                                   false,
                                   PrefService::UNSYNCABLE_PREF);
  local_state->RegisterStringPref(kInitialLocale,
                                  "en-US",
                                  PrefService::UNSYNCABLE_PREF);
  // Check if the pref is already registered in case
  // Preferences::RegisterUserPrefs runs before this code in the future.
  if (local_state->FindPreference(prefs::kAccessibilityEnabled) == NULL) {
    local_state->RegisterBooleanPref(prefs::kAccessibilityEnabled,
                                     false,
                                     PrefService::UNSYNCABLE_PREF);
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnNetworkConnected() {
  if (is_official_build_) {
    if (!IsEulaAccepted()) {
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
  ShowLoginScreen();
}

void WizardController::OnConnectionFailed() {
  // TODO(dpolukhin): show error message after login screen is displayed.
  ShowLoginScreen();
}

void WizardController::OnUpdateCompleted() {
  OnOOBECompleted();
}

void WizardController::OnEulaAccepted() {
  MarkEulaAccepted();
  // TODO(pastarmovj): Make this code cache the value for the pref in a better
  // way until we can store it in the policy blob. See explanation below:
  // At this point we can not write this in the signed settings pref blob.
  // But we can at least create the consent file and Chrome would port that
  // if the device is owned by a local user. In case of enterprise enrolled
  // device the setting will be respected only until the policy is not set.
  base::FundamentalValue value(usage_statistics_reporting_);
  SignedSettingsTempStorage::Store(
      kStatsReportingPref,
      value,
      g_browser_process->local_state());
  bool enabled =
      OptionsUtil::ResolveMetricsReportingEnabled(usage_statistics_reporting_);
  // Make sure the local state cached value is updated too because the real
  // policy will only get written when the owner is created and the cache won't
  // be updated until the policy is reread.
  g_browser_process->local_state()->SetBoolean(kStatsReportingPref, enabled);
  if (enabled) {
#if defined(USE_LINUX_BREAKPAD)
    // The crash reporter initialization needs IO to complete.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    InitCrashReporter();
#endif
  }

  InitiateOOBEUpdate();
}

void WizardController::OnUpdateErrorCheckingForUpdate() {
  // TODO(nkostylev): Update should be required during OOBE.
  // We do not want to block users from being able to proceed to the login
  // screen if there is any error checking for an update.
  // They could use "browse without sign-in" feature to set up the network to be
  // able to perform the update later.
  OnOOBECompleted();
}

void WizardController::OnUpdateErrorUpdating() {
  // If there was an error while getting or applying the update,
  // return to network selection screen.
  // TODO(nkostylev): Show message to the user explaining update error.
  // TODO(nkostylev): Update should be required during OOBE.
  // Temporary fix, need to migrate to new API. http://crosbug.com/4321
  OnOOBECompleted();
}

void WizardController::OnUserImageSelected() {
  // Launch browser and delete login host controller.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&chromeos::LoginUtils::DoBrowserLaunch,
                 ProfileManager::GetDefaultProfile(), host_));
  host_ = NULL;
  // TODO(avayvod): Sync image with Google Sync.
}

void WizardController::OnUserImageSkipped() {
  OnUserImageSelected();
}

void WizardController::OnRegistrationSuccess() {
  MarkDeviceRegistered();
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest()) {
    chromeos::LoginUtils::Get()->CompleteOffTheRecordLogin(start_url_);
  } else {
    ShowUserImageScreen();
  }
}

void WizardController::OnRegistrationSkipped() {
  // TODO(nkostylev): Track in a histogram?
  OnRegistrationSuccess();
}

void WizardController::OnEnterpriseEnrollmentDone() {
  ShowLoginScreen();
}

void WizardController::OnOOBECompleted() {
  MarkOobeCompleted();
  ShowLoginScreen();
}

void WizardController::InitiateOOBEUpdate() {
  GetUpdateScreen()->StartUpdate();
  SetCurrentScreenSmooth(GetUpdateScreen(), true);
}


void WizardController::SetCurrentScreen(WizardScreen* new_current) {
  SetCurrentScreenSmooth(new_current, false);
}

void WizardController::ShowCurrentScreen() {
  // ShowCurrentScreen may get called by smooth_show_timer_ even after
  // flow has been switched to sign in screen (ExistingUserController).
  if (!oobe_display_)
    return;

  smooth_show_timer_.Stop();

  oobe_display_->ShowScreen(current_screen_);
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
    oobe_display_->HideScreen(current_screen_);

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

void WizardController::ShowFirstScreen(const std::string& first_screen_name) {
  if (first_screen_name == kNetworkScreenName) {
    ShowNetworkScreen();
  } else if (first_screen_name == kLoginScreenName) {
    ShowLoginScreen();
  } else if (first_screen_name == kUpdateScreenName) {
    InitiateOOBEUpdate();
  } else if (first_screen_name == kUserImageScreenName) {
    ShowUserImageScreen();
  } else if (first_screen_name == kEulaScreenName) {
    ShowEulaScreen();
  } else if (first_screen_name == kRegistrationScreenName) {
    if (is_official_build_) {
      ShowRegistrationScreen();
    } else {
      // Just proceed to image screen.
      OnRegistrationSuccess();
    }
  } else if (first_screen_name == kHTMLPageScreenName) {
    ShowHTMLPageScreen();
  } else if (first_screen_name == kEnterpriseEnrollmentScreenName) {
    ShowEnterpriseEnrollmentScreen();
  } else if (first_screen_name != kTestNoScreenName) {
    if (is_out_of_box_) {
      ShowNetworkScreen();
    } else {
      ShowLoginScreen();
    }
  }
}

// static
bool WizardController::IsEulaAccepted() {
  return g_browser_process->local_state()->GetBoolean(kEulaAccepted);
}

// static
bool WizardController::IsOobeCompleted() {
  return g_browser_process->local_state()->GetBoolean(kOobeComplete);
}

// static
void WizardController::MarkEulaAccepted() {
  SaveBoolPreferenceForced(kEulaAccepted, true);
}

// static
void WizardController::MarkOobeCompleted() {
  SaveBoolPreferenceForced(kOobeComplete, true);
}

static void CreateOobeCompleteFlagFile() {
  // Create flag file for boot-time init scripts.
  FilePath oobe_complete_path(kOobeCompleteFlagFilePath);
  if (!file_util::PathExists(oobe_complete_path)) {
    FILE* oobe_flag_file = file_util::OpenFile(oobe_complete_path, "w+b");
    if (oobe_flag_file == NULL)
      DLOG(WARNING) << kOobeCompleteFlagFilePath << " doesn't exist.";
    else
      file_util::CloseFile(oobe_flag_file);
  }
}

// static
bool WizardController::IsDeviceRegistered() {
  int value = g_browser_process->local_state()->GetInteger(kDeviceRegistered);
  if (value > 0) {
    // Recreate flag file in case it was lost.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CreateOobeCompleteFlagFile));
    return true;
  } else if (value == 0) {
    return false;
  } else {
    // Pref is not set. For compatibility check flag file. It causes blocking
    // IO on UI thread. But it's required for update from old versions.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    FilePath oobe_complete_flag_file_path(kOobeCompleteFlagFilePath);
    bool file_exists = file_util::PathExists(oobe_complete_flag_file_path);
    SaveIntegerPreferenceForced(kDeviceRegistered, file_exists ? 1 : 0);
    return file_exists;
  }
}

// static
void WizardController::MarkDeviceRegistered() {
  SaveIntegerPreferenceForced(kDeviceRegistered, 1);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateOobeCompleteFlagFile));
}

// static
std::string WizardController::GetInitialLocale() {
  std::string locale =
      g_browser_process->local_state()->GetString(kInitialLocale);
  if (!l10n_util::IsValidLocaleSyntax(locale))
    locale = "en-US";
  return locale;
}

// static
void WizardController::SetInitialLocale(const std::string& locale) {
  if (l10n_util::IsValidLocaleSyntax(locale))
    SaveStringPreferenceForced(kInitialLocale, locale);
  else
    NOTREACHED();
}

// static
bool WizardController::IsRegisterScreenDefined() {
  const chromeos::StartupCustomizationDocument* manifest =
      chromeos::StartupCustomizationDocument::GetInstance();
  return manifest->IsReady() &&
         GURL(manifest->registration_url()).is_valid();
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, chromeos::ScreenObserver overrides:
void WizardController::OnExit(ExitCodes exit_code) {
  LOG(INFO) << "Wizard screen exit code: " << exit_code;
  switch (exit_code) {
    case NETWORK_CONNECTED:
      OnNetworkConnected();
      break;
    case NETWORK_OFFLINE:
      OnNetworkOffline();
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
    case USER_IMAGE_SKIPPED:
      OnUserImageSkipped();
      break;
    case EULA_ACCEPTED:
      OnEulaAccepted();
      break;
    case EULA_BACK:
      ShowNetworkScreen();
      break;
    case REGISTRATION_SUCCESS:
      OnRegistrationSuccess();
      break;
    case REGISTRATION_SKIPPED:
      OnRegistrationSkipped();
      break;
    case ENTERPRISE_ENROLLMENT_CANCELLED:
    case ENTERPRISE_ENROLLMENT_COMPLETED:
      OnEnterpriseEnrollmentDone();
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

void WizardController::set_usage_statistics_reporting(bool val) {
  usage_statistics_reporting_ = val;
}

bool WizardController::usage_statistics_reporting() const {
  return usage_statistics_reporting_;
}

void WizardController::SetZeroDelays() {
  kShowDelayMs = 0;
}

}  // namespace chromeos
