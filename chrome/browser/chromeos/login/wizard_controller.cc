// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <gdk/gdk.h>
#include <signal.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/eula_view.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/html_page_screen.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/registration_screen.h"
#include "chrome/browser/chromeos/login/update_screen.h"
#include "chrome/browser/chromeos/login/user_image_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/metrics_cros_settings_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/accelerator.h"
#include "views/view.h"

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

// RootView of the Widget WizardController creates. Contains the contents of the
// WizardController.
class ContentView : public views::View {
 public:
  ContentView()
      : accel_toggle_accessibility_(
            chromeos::WizardAccessibilityHelper::GetAccelerator()) {
#if defined(OFFICIAL_BUILD)
    accel_cancel_update_ =  views::Accelerator(ui::VKEY_ESCAPE,
                                               true, true, true);
#else
    accel_cancel_update_ =  views::Accelerator(ui::VKEY_ESCAPE,
                                               false, false, false);
    accel_account_screen_ = views::Accelerator(ui::VKEY_A,
                                               false, true, true);
    accel_login_screen_ = views::Accelerator(ui::VKEY_L,
                                             false, true, true);
    accel_network_screen_ = views::Accelerator(ui::VKEY_N,
                                               false, true, true);
    accel_update_screen_ = views::Accelerator(ui::VKEY_U,
                                              false, true, true);
    accel_image_screen_ = views::Accelerator(ui::VKEY_I,
                                             false, true, true);
    accel_eula_screen_ = views::Accelerator(ui::VKEY_E,
                                            false, true, true);
    accel_register_screen_ = views::Accelerator(ui::VKEY_R,
                                                false, true, true);
    accel_enterprise_enrollment_screen_ =
        views::Accelerator(ui::VKEY_P, false, true, true);
    AddAccelerator(accel_account_screen_);
    AddAccelerator(accel_login_screen_);
    AddAccelerator(accel_network_screen_);
    AddAccelerator(accel_update_screen_);
    AddAccelerator(accel_image_screen_);
    AddAccelerator(accel_eula_screen_);
    AddAccelerator(accel_register_screen_);
    AddAccelerator(accel_enterprise_enrollment_screen_);
#endif
    AddAccelerator(accel_toggle_accessibility_);
    AddAccelerator(accel_cancel_update_);
  }

  ~ContentView() {
    NotificationService::current()->Notify(
        NotificationType::WIZARD_CONTENT_VIEW_DESTROYED,
        NotificationService::AllSources(),
        NotificationService::NoDetails());
  }

  bool AcceleratorPressed(const views::Accelerator& accel) {
    WizardController* controller = WizardController::default_controller();
    if (!controller)
      return false;

    if (accel == accel_toggle_accessibility_) {
      chromeos::WizardAccessibilityHelper::GetInstance()->ToggleAccessibility();
    } else if (accel == accel_cancel_update_) {
      controller->CancelOOBEUpdate();
#if !defined(OFFICIAL_BUILD)
    } else if (accel == accel_account_screen_) {
      controller->ShowAccountScreen();
    } else if (accel == accel_login_screen_) {
      controller->ShowLoginScreen();
    } else if (accel == accel_network_screen_) {
      controller->ShowNetworkScreen();
    } else if (accel == accel_update_screen_) {
      controller->ShowUpdateScreen();
    } else if (accel == accel_image_screen_) {
      controller->ShowUserImageScreen();
    } else if (accel == accel_eula_screen_) {
      controller->ShowEulaScreen();
    } else if (accel == accel_register_screen_) {
      controller->ShowRegistrationScreen();
    } else if (accel == accel_enterprise_enrollment_screen_) {
      controller->ShowEnterpriseEnrollmentScreen();
#endif
    } else {
      return false;
    }

    return true;
  }

  virtual void Layout() {
    for (int i = 0; i < child_count(); ++i) {
      views::View* cur = GetChildViewAt(i);
      if (cur->IsVisible())
        cur->SetBounds(0, 0, width(), height());
    }
  }

 private:
#if !defined(OFFICIAL_BUILD)
  views::Accelerator accel_account_screen_;
  views::Accelerator accel_login_screen_;
  views::Accelerator accel_network_screen_;
  views::Accelerator accel_update_screen_;
  views::Accelerator accel_image_screen_;
  views::Accelerator accel_eula_screen_;
  views::Accelerator accel_register_screen_;
  views::Accelerator accel_enterprise_enrollment_screen_;
#endif
  views::Accelerator accel_toggle_accessibility_;
  views::Accelerator accel_cancel_update_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

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

const char WizardController::kNetworkScreenName[] = "network";
const char WizardController::kLoginScreenName[] = "login";
const char WizardController::kAccountScreenName[] = "account";
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
                                   const gfx::Rect& screen_bounds)
    : widget_(NULL),
      contents_(NULL),
      screen_bounds_(screen_bounds),
      current_screen_(NULL),
      initial_show_(true),
      is_active_(true),
#if defined(OFFICIAL_BUILD)
      is_official_build_(true),
#else
      is_official_build_(false),
#endif
      is_out_of_box_(false),
      host_(host),
      observer_(NULL),
      usage_statistics_reporting_(true) {
  DCHECK(default_controller_ == NULL);
  default_controller_ = this;
}

WizardController::~WizardController() {
  if (widget_) {
    widget_->Close();
    widget_ = NULL;
  }

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
  DCHECK(!contents_);
  first_screen_name_ = first_screen_name;

  contents_ = new ContentView();

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
    network_screen_.reset(new chromeos::NetworkScreen(this));
  return network_screen_.get();
}

chromeos::AccountScreen* WizardController::GetAccountScreen() {
  if (!account_screen_.get())
    account_screen_.reset(new chromeos::AccountScreen(this));
  return account_screen_.get();
}

chromeos::UpdateScreen* WizardController::GetUpdateScreen() {
  if (!update_screen_.get()) {
    update_screen_.reset(new chromeos::UpdateScreen(this));
    update_screen_->SetRebootCheckDelay(kWaitForRebootTimeSec);
  }
  return update_screen_.get();
}

chromeos::UserImageScreen* WizardController::GetUserImageScreen() {
  if (!user_image_screen_.get())
    user_image_screen_.reset(new chromeos::UserImageScreen(this));
  return user_image_screen_.get();
}

chromeos::EulaScreen* WizardController::GetEulaScreen() {
  if (!eula_screen_.get())
    eula_screen_.reset(new chromeos::EulaScreen(this));
  return eula_screen_.get();
}

chromeos::RegistrationScreen* WizardController::GetRegistrationScreen() {
  if (!registration_screen_.get())
    registration_screen_.reset(new chromeos::RegistrationScreen(this));
  return registration_screen_.get();
}

chromeos::HTMLPageScreen* WizardController::GetHTMLPageScreen() {
  if (!html_page_screen_.get()) {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    std::string url;
    // It's strange but args may contains empty strings.
    for (size_t i = 0; i < command_line->args().size(); i++) {
      if (!command_line->args()[i].empty()) {
        DCHECK(url.empty()) << "More than one URL in command line";
        url = command_line->args()[i];
      }
    }
    DCHECK(!url.empty()) << "No URL in commane line";
    html_page_screen_.reset(new chromeos::HTMLPageScreen(this, url));
  }
  return html_page_screen_.get();
}

chromeos::EnterpriseEnrollmentScreen*
    WizardController::GetEnterpriseEnrollmentScreen() {
  if (!enterprise_enrollment_screen_.get()) {
    enterprise_enrollment_screen_.reset(
        new chromeos::EnterpriseEnrollmentScreen(this));
  }
  return enterprise_enrollment_screen_.get();
}

void WizardController::ShowNetworkScreen() {
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetNetworkScreen());
  host_->SetOobeProgress(chromeos::BackgroundView::SELECT_NETWORK);
}

void WizardController::ShowLoginScreen() {
  SetStatusAreaVisible(true);
  host_->SetOobeProgress(chromeos::BackgroundView::SIGNIN);
  host_->StartSignInScreen();
  smooth_show_timer_.Stop();
  if (widget_) {
    widget_->Close();
    widget_ = NULL;
  }
  is_active_ = false;
}

void WizardController::ShowAccountScreen() {
  VLOG(1) << "Showing create account screen.";
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetAccountScreen());
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

void WizardController::OnAccountCreateBack() {
  ShowLoginScreen();
}

void WizardController::OnAccountCreated() {
  ShowLoginScreen();
  // TODO(dpolukhin): clear password memory for real. Now it is not
  // a problem because we can't extract password from the form.
  password_.clear();
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
  chromeos::MetricsCrosSettingsProvider::SetMetricsStatus(
      usage_statistics_reporting_);
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    // TPM password could be seen on EULA screen, now it's safe to clear it.
    chromeos::CrosLibrary::Get()->
        GetCryptohomeLibrary()->TpmClearStoredPassword();
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
      NewRunnableFunction(&chromeos::LoginUtils::DoBrowserLaunch,
                          ProfileManager::GetDefaultProfile(),
                          host_));
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

///////////////////////////////////////////////////////////////////////////////
// WizardController, private:

views::Widget* WizardController::CreateScreenWindow(
    const gfx::Rect& bounds, bool initial_show) {
  widget_ = new views::Widget;
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  // Window transparency makes background flicker through controls that
  // are constantly updating its contents (like image view with video
  // stream). Hence enabling double buffer.
  widget_params.double_buffer = true;
  widget_params.transparent = true;
  widget_params.bounds = bounds;
  widget_->Init(widget_params);
  std::vector<int> params;
  // For initial show WM would animate background window.
  // Otherwise it stays unchanged.
  params.push_back(initial_show);
  chromeos::WmIpc::instance()->SetWindowType(
      widget_->GetNativeView(),
      chromeos::WM_IPC_WINDOW_LOGIN_GUEST,
      &params);
  widget_->SetContentsView(contents_);
  return widget_;
}

gfx::Rect WizardController::GetWizardScreenBounds(int screen_width,
                                                  int screen_height) const {
  int offset_x = (screen_bounds_.width() - screen_width) / 2;
  int offset_y = (screen_bounds_.height() - screen_height) / 2;
  int window_x = screen_bounds_.x() + offset_x;
  int window_y = screen_bounds_.y() + offset_y;
  return gfx::Rect(window_x, window_y, screen_width, screen_height);
}


void WizardController::SetCurrentScreen(WizardScreen* new_current) {
  SetCurrentScreenSmooth(new_current, false);
}

void WizardController::ShowCurrentScreen() {
  // ShowCurrentScreen may get called by smooth_show_timer_ even after
  // flow has been switched to sign in screen (ExistingUserController).
  if (!is_active_)
    return;

  smooth_show_timer_.Stop();

  bool force_widget_show = false;
  views::Widget* window = NULL;

  gfx::Rect current_bounds;
  if (widget_)
    current_bounds = widget_->GetClientAreaScreenBounds();
  gfx::Size new_screen_size = current_screen_->GetScreenSize();
  gfx::Rect new_bounds = GetWizardScreenBounds(new_screen_size.width(),
                                               new_screen_size.height());
  if (new_bounds != current_bounds) {
    if (widget_)
      widget_->Close();
    force_widget_show = true;
    window = CreateScreenWindow(new_bounds, initial_show_);
  }
  current_screen_->Show();
  contents_->Layout();
  contents_->SchedulePaint();
  if (force_widget_show) {
    // This keeps the window from flashing at startup.
    GdkWindow* gdk_window = window->GetNativeView()->window;
    gdk_window_set_back_pixmap(gdk_window, NULL, false);
    if (widget_)
      widget_->Show();
  }
}

void WizardController::SetCurrentScreenSmooth(WizardScreen* new_current,
                                              bool use_smoothing) {
  if (current_screen_ == new_current || new_current == NULL)
    return;

  smooth_show_timer_.Stop();

  if (current_screen_) {
    initial_show_ = false;
    current_screen_->Hide();
  }

  current_screen_ = new_current;

  if (use_smoothing) {
    smooth_show_timer_.Start(
        base::TimeDelta::FromMilliseconds(kShowDelayMs),
        this,
        &WizardController::ShowCurrentScreen);
    contents_->Layout();
    contents_->SchedulePaint();
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
  } else if (first_screen_name == kAccountScreenName) {
    ShowAccountScreen();
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
        NewRunnableFunction(&CreateOobeCompleteFlagFile));
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
      NewRunnableFunction(&CreateOobeCompleteFlagFile));
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
    case ACCOUNT_CREATE_BACK:
      OnAccountCreateBack();
      break;
    case ACCOUNT_CREATED:
      OnAccountCreated();
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

///////////////////////////////////////////////////////////////////////////////
// WizardController, WizardScreen overrides:
views::View* WizardController::GetWizardView() {
  return contents_;
}

chromeos::ScreenObserver* WizardController::GetObserver(WizardScreen* screen) {
  return observer_ ? observer_ : this;
}

void WizardController::SetZeroDelays() {
  kShowDelayMs = 0;
}
