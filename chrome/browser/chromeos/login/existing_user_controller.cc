// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/views_login_display.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/window/window.h"

namespace chromeos {

namespace {

// Url for setting up sync authentication.
const char kSettingsSyncLoginURL[] = "chrome://settings/personal";

// URL that will be opened on when user logs in first time on the device.
const char kGetStartedURL[] =
    "chrome-extension://cbmhffdpiobpchciemffincgahkkljig/index.html";

// URL for account creation.
const char kCreateAccountURL[] =
    "https://www.google.com/accounts/NewAccount?service=mail";

// Used to handle the asynchronous response of deleting a cryptohome directory.
class RemoveAttempt : public CryptohomeLibrary::Delegate {
 public:
  explicit RemoveAttempt(const std::string& user_email)
      : user_email_(user_email) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      CrosLibrary::Get()->GetCryptohomeLibrary()->AsyncRemove(
          user_email_, this);
    }
  }

  void OnComplete(bool success, int return_code) {
    // Log the error, but there's not much we can do.
    if (!success) {
      VLOG(1) << "Removal of cryptohome for " << user_email_
              << " failed, return code: " << return_code;
    }
    delete this;
  }

 private:
  std::string user_email_;
};

}  // namespace

// static
ExistingUserController*
  ExistingUserController::current_controller_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, public:

ExistingUserController::ExistingUserController(
    const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      background_window_(NULL),
      background_view_(NULL),
      num_login_attempts_(0),
      user_settings_(new UserCrosSettingsProvider),
      method_factory_(this) {
  if (current_controller_)
    current_controller_->Delete();
  current_controller_ = this;

  login_display_.reset(CreateLoginDisplay(this, background_bounds));

  registrar_.Add(this,
                 NotificationType::LOGIN_USER_IMAGE_CHANGED,
                 NotificationService::AllSources());
}

void ExistingUserController::Init(const UserVector& users) {
  if (!background_window_) {
    background_window_ = BackgroundView::CreateWindowContainingView(
        background_bounds_,
        GURL(),
        &background_view_);
    background_view_->EnableShutdownButton(true);

    if (!WizardController::IsDeviceRegistered()) {
      background_view_->SetOobeProgressBarVisible(true);
      background_view_->SetOobeProgress(chromeos::BackgroundView::SIGNIN);
    }

    background_window_->Show();
  }

  UserVector filtered_users;
  if (UserCrosSettingsProvider::cached_show_users_on_signin()) {
    for (size_t i = 0; i < users.size(); ++i)
      // TODO(xiyuan): Clean user profile whose email is not in whitelist.
      if (UserCrosSettingsProvider::cached_allow_new_user() ||
          UserCrosSettingsProvider::IsEmailInCachedWhitelist(
              users[i].email())) {
        filtered_users.push_back(users[i]);
      }
  }

  // If no user pods are visible, fallback to single new user pod which will
  // have guest session link.
  bool show_guest = UserCrosSettingsProvider::cached_allow_guest() &&
                    !filtered_users.empty();
  bool show_new_user = true;
  login_display_->set_parent_window(GetNativeWindow());
  login_display_->Init(filtered_users, show_guest, show_new_user);

  WmMessageListener::GetInstance()->AddObserver(this);

  LoginUtils::Get()->PrewarmAuthentication();
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetLoginLibrary()->EmitLoginPromptReady();
}

void ExistingUserController::OwnBackground(
    views::Widget* background_widget,
    chromeos::BackgroundView* background_view) {
  DCHECK(!background_window_);
  background_window_ = background_widget;
  background_view_ = background_view;
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, NotificationObserver implementation:
//

void ExistingUserController::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (type != NotificationType::LOGIN_USER_IMAGE_CHANGED)
    return;

  UserManager::User* user = Details<UserManager::User>(details).ptr();
  login_display_->OnUserImageChanged(user);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  if (background_window_)
    background_window_->Close();

  WmMessageListener::GetInstance()->RemoveObserver(this);

  DCHECK(current_controller_ != NULL);
  current_controller_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, WmMessageListener::Observer implementation:
//

void ExistingUserController::ProcessWmMessage(const WmIpc::Message& message,
                                              GdkWindow* window) {
  if (message.type() != WM_IPC_MESSAGE_CHROME_CREATE_GUEST_WINDOW)
    return;

  ActivateWizard(std::string());
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginDisplay::Delegate implementation:
//

void ExistingUserController::CreateAccount() {
  guest_mode_url_ =
      google_util::AppendGoogleLocaleParam(GURL(kCreateAccountURL));
  LoginAsGuest();
}

void ExistingUserController::Login(const std::string& username,
                                   const std::string& password) {
  if (username.empty() || password.empty())
    return;
  SetStatusAreaEnabled(false);
  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  BootTimesLoader::Get()->RecordLoginAttempted();

  if (last_login_attempt_username_ != username) {
    last_login_attempt_username_ = username;
    num_login_attempts_ = 0;
  }
  num_login_attempts_++;

  // Use the same LoginPerformer for subsequent login as it has state
  // such as CAPTCHA challenge token & corresponding user input.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    LoginPerformer::Delegate* delegate = this;
    if (login_performer_delegate_.get())
      delegate = login_performer_delegate_.get();
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(NULL);
    login_performer_.reset(new LoginPerformer(delegate));
  }
  login_performer_->Login(username, password);
  WizardAccessibilityHelper::GetInstance()->MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNING_IN).c_str(),
      false, true);
}

void ExistingUserController::LoginAsGuest() {
  SetStatusAreaEnabled(false);
  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  // Check allow_guest in case this call is fired from key accelerator.
  // Must not proceed without signature verification.
  bool trusted_setting_available = user_settings_->RequestTrustedAllowGuest(
      method_factory_.NewRunnableMethod(
          &ExistingUserController::LoginAsGuest));
  if (!trusted_setting_available) {
    // Value of AllowGuest setting is still not verified.
    // Another attempt will be invoked again after verification completion.
    return;
  }
  if (!UserCrosSettingsProvider::cached_allow_guest()) {
    // Disallowed.
    return;
  }

  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(NULL);
  login_performer_.reset(new LoginPerformer(this));
  login_performer_->LoginOffTheRecord();
  WizardAccessibilityHelper::GetInstance()->MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_OFFRECORD).c_str(),
      false, true);
}

void ExistingUserController::OnUserSelected(const std::string& username) {
  login_performer_.reset(NULL);
  num_login_attempts_ = 0;
}

void ExistingUserController::RemoveUser(const std::string& username) {
  // Owner is not allowed to be removed from the device.
  // Must not proceed without signature verification.
  UserCrosSettingsProvider user_settings;
  bool trusted_owner_available = user_settings.RequestTrustedOwner(
      method_factory_.NewRunnableMethod(&ExistingUserController::RemoveUser,
                                        username));
  if (!trusted_owner_available) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }
  if (username == UserCrosSettingsProvider::cached_owner()) {
    // Owner is not allowed to be removed from the device.
    return;
  }

  login_display_->OnBeforeUserRemoved(username);

  // Delete user from user list.
  UserManager::Get()->RemoveUser(username);

  // Delete the encrypted user directory.
  new RemoveAttempt(username);

  login_display_->OnUserRemoved(username);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnLoginFailure(const LoginFailure& failure) {
  guest_mode_url_ = GURL::EmptyGURL();
  std::string error = failure.GetErrorString();

  // Check networking after trying to login in case user is
  // cached locally or the local admin account.
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
  if (!network || !CrosLibrary::Get()->EnsureLoaded()) {
    ShowError(IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY, error);
  } else if (!network->Connected()) {
    ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
  } else {
    if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
        failure.error().state() == GoogleServiceAuthError::CAPTCHA_REQUIRED) {
      if (!failure.error().captcha().image_url.is_empty()) {
        CaptchaView* view =
            new CaptchaView(failure.error().captcha().image_url, false);
        view->Init();
        view->set_delegate(this);
        views::Window* window = browser::CreateViewsWindow(
            GetNativeWindow(), gfx::Rect(), view);
        window->SetIsAlwaysOnTop(true);
        window->Show();
      } else {
        LOG(WARNING) << "No captcha image url was found?";
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      }
    } else if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
               failure.error().state() ==
                   GoogleServiceAuthError::HOSTED_NOT_ALLOWED) {
      ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED, error);
    } else {
      if (!UserManager::Get()->IsKnownUser(last_login_attempt_username_))
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_NEW, error);
      else
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
    }
  }

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);
  SetStatusAreaEnabled(true);
}

void ExistingUserController::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {
  // LoginPerformer instance will delete itself once online auth result is OK.
  // In case of failure it'll bring up ScreenLock and ask for
  // correct password/display error message.
  // Even in case when following online,offline protocol and returning
  // requests_pending = false, let LoginPerformer delete itself.
  login_performer_->set_delegate(NULL);
  LoginPerformer* performer = login_performer_.release();
  performer = NULL;
  bool known_user = UserManager::Get()->IsKnownUser(username);
  bool login_only =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kLoginScreen) == WizardController::kLoginScreenName;
  // TODO(nkostylev): May add login UI implementation callback call.
  if (!known_user && !login_only) {
#if defined(OFFICIAL_BUILD)
    CommandLine::ForCurrentProcess()->AppendArg(kGetStartedURL);
#endif  // OFFICIAL_BUILD

    if (credentials.two_factor) {
      // If we have a two factor error and and this is a new user,
      // load the personal settings page.
      // TODO(stevenjb): direct the user to a lightweight sync login page.
      CommandLine::ForCurrentProcess()->AppendArg(kSettingsSyncLoginURL);
    }

    // For new user login don't launch browser until we pass image screen.
    LoginUtils::Get()->EnableBrowserLaunch(false);
    LoginUtils::Get()->CompleteLogin(username,
                                     password,
                                     credentials,
                                     pending_requests);
    ActivateWizard(WizardController::IsDeviceRegistered() ?
        WizardController::kUserImageScreenName :
        WizardController::kRegistrationScreenName);
  } else {
    // Hide the login windows now.
    WmIpc::Message message(WM_IPC_MESSAGE_WM_HIDE_LOGIN);
    WmIpc::instance()->SendMessage(message);

    LoginUtils::Get()->CompleteLogin(username,
                                     password,
                                     credentials,
                                     pending_requests);

    // Delay deletion as we're on the stack.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void ExistingUserController::OnOffTheRecordLoginSuccess() {
  if (WizardController::IsDeviceRegistered()) {
    LoginUtils::Get()->CompleteOffTheRecordLogin(guest_mode_url_);
  } else {
    // Postpone CompleteOffTheRecordLogin until registration completion.
    ActivateWizard(WizardController::kRegistrationScreenName);
  }
}

void ExistingUserController::OnPasswordChangeDetected(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Must not proceed without signature verification.
  bool trusted_setting_available = user_settings_->RequestTrustedOwner(
      method_factory_.NewRunnableMethod(
          &ExistingUserController::OnPasswordChangeDetected,
          credentials));
  if (!trusted_setting_available) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }
  // TODO(altimofeev): remove this constrain when full sync for the owner will
  // be correctly handled.
  bool full_sync_disabled = (UserCrosSettingsProvider::cached_owner() ==
      last_login_attempt_username_);

  PasswordChangedView* view = new PasswordChangedView(this, full_sync_disabled);
  views::Window* window = browser::CreateViewsWindow(GetNativeWindow(),
                                                     gfx::Rect(),
                                                     view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void ExistingUserController::WhiteListCheckFailed(const std::string& email) {
  ShowError(IDS_LOGIN_ERROR_WHITELIST, email);

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);
  SetStatusAreaEnabled(true);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, CaptchaView::Delegate implementation:
//

void ExistingUserController::OnCaptchaEntered(const std::string& captcha) {
  login_performer_->set_captcha(captcha);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, PasswordChangedView::Delegate implementation:
//

void ExistingUserController::RecoverEncryptedData(
    const std::string& old_password) {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get())
    login_performer_->RecoverEncryptedData(old_password);
}

void ExistingUserController::ResyncEncryptedData() {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get())
    login_performer_->ResyncEncryptedData();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

void ExistingUserController::ActivateWizard(const std::string& screen_name) {
  // WizardController takes care of deleting itself when done.
  WizardController* controller = new WizardController();

  // Give the background window to the controller.
  controller->OwnBackground(background_window_, background_view_);
  background_window_ = NULL;

  controller->Init(screen_name, background_bounds_);
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest())
    controller->set_start_url(guest_mode_url_);

  delete_timer_.Start(base::TimeDelta::FromSeconds(1), this,
                      &ExistingUserController::Delete);
}

LoginDisplay* ExistingUserController::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate, const gfx::Rect& background_bounds) {
  // TODO(rharrison): Create Web UI implementation too. http://crosbug.com/6398.
  return new ViewsLoginDisplay(delegate, background_bounds);
}

void ExistingUserController::Delete() {
  delete this;
}

gfx::NativeWindow ExistingUserController::GetNativeWindow() const {
  return background_view_->GetNativeWindow();
}

void ExistingUserController::SetStatusAreaEnabled(bool enable) {
  if (background_view_) {
    background_view_->SetStatusAreaEnabled(enable);
  }
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  // TODO(dpolukhin): show detailed error info. |details| string contains
  // low level error info that is not localized and even is not user friendly.
  // For now just ignore it because error_text contains all required information
  // for end users, developers can see details string in Chrome logs.
  VLOG(1) << details;
  HelpAppLauncher::HelpTopic help_topic_id;
  switch (login_performer_->error().state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
      help_topic_id = HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE;
      break;
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      help_topic_id = HelpAppLauncher::HELP_ACCOUNT_DISABLED;
      break;
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
      help_topic_id = HelpAppLauncher::HELP_HOSTED_ACCOUNT;
      break;
    default:
      help_topic_id = login_performer_->login_timed_out() ?
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE :
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT;
      break;
  }

  login_display_->ShowError(error_id, num_login_attempts_, help_topic_id);
}

}  // namespace chromeos
