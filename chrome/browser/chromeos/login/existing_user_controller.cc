// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/widget/widget.h"

namespace chromeos {

namespace {

// Url for setting up sync authentication.
const char kSettingsSyncLoginURL[] = "chrome://settings/personal";

// URL that will be opened when user logs in first time on the device.
const char kGetStartedURLPattern[] =
    "http://www.gstatic.com/chromebook/gettingstarted/index-%s.html";

// URL that will be opened when first user logs in first time on the device.
const char kGetStartedOwnerURLPattern[] =
    "http://www.gstatic.com/chromebook/gettingstarted/index-%s.html#first";

// URL for account creation.
const char kCreateAccountURL[] =
    "https://www.google.com/accounts/NewAccount?service=mail";

// ChromeVox tutorial URL.
const char kChromeVoxTutorialURL[] =
    "http://google-axs-chrome.googlecode.com/"
    "svn/trunk/chromevox_tutorial/interactive_tutorial_start.html";

// Landing URL when launching Guest mode to fix captive portal.
const char kCaptivePortalLaunchURL[] = "http://www.google.com/";

}  // namespace

// static
ExistingUserController* ExistingUserController::current_controller_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, public:

ExistingUserController::ExistingUserController(LoginDisplayHost* host)
    : login_status_consumer_(NULL),
      host_(host),
      login_display_(host_->CreateLoginDisplay(this)),
      num_login_attempts_(0),
      user_settings_(new UserCrosSettingsProvider),
      weak_factory_(this),
      is_owner_login_(false) {
  DCHECK(current_controller_ == NULL);
  current_controller_ = this;

  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                 content::NotificationService::AllSources());
}

void ExistingUserController::Init(const UserList& users) {
  UserList filtered_users;
  if (UserCrosSettingsProvider::cached_show_users_on_signin()) {
    for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
      // TODO(xiyuan): Clean user profile whose email is not in whitelist.
      if (UserCrosSettingsProvider::cached_allow_new_user() ||
          UserCrosSettingsProvider::IsEmailInCachedWhitelist((*it)->email())) {
        filtered_users.push_back(*it);
      }
    }
  }

  // If no user pods are visible, fallback to single new user pod which will
  // have guest session link.
  bool show_guest = UserCrosSettingsProvider::cached_allow_guest() &&
                    !filtered_users.empty();
  bool show_new_user = true;
  login_display_->set_parent_window(GetNativeWindow());
  login_display_->Init(filtered_users, show_guest, show_new_user);

  LoginUtils::Get()->PrewarmAuthentication();
  DBusThreadManager::Get()->GetSessionManagerClient()->EmitLoginPromptReady();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, content::NotificationObserver implementation:
//

void ExistingUserController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED)
    return;
  login_display_->OnUserImageChanged(*content::Details<User>(details).ptr());
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  LoginUtils::Get()->DelegateDeleted(this);

  if (current_controller_ == this) {
    current_controller_ = NULL;
  } else {
    NOTREACHED() << "More than one controller are alive.";
  }
  DCHECK(login_display_.get());
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginDisplay::Delegate implementation:
//

void ExistingUserController::CreateAccount() {
  guest_mode_url_ =
      google_util::AppendGoogleLocaleParam(GURL(kCreateAccountURL));
  LoginAsGuest();
}

string16 ExistingUserController::GetConnectedNetworkName() {
  return GetCurrentNetworkName(CrosLibrary::Get()->GetNetworkLibrary());
}

void ExistingUserController::FixCaptivePortal() {
  guest_mode_url_ = GURL(kCaptivePortalLaunchURL);
  LoginAsGuest();
}

void ExistingUserController::CompleteLogin(const std::string& username,
                                           const std::string& password) {
  SetOwnerUserInCryptohome();

  GaiaAuthConsumer::ClientLoginResult credentials;
  if (!login_performer_.get()) {
    LoginPerformer::Delegate* delegate = this;
    if (login_performer_delegate_.get())
      delegate = login_performer_delegate_.get();
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(new LoginPerformer(delegate));
  }

  // If the device is not owned yet, successfully logged in user will be owner.
  is_owner_login_ = OwnershipService::GetSharedInstance()->GetStatus(true) ==
      OwnershipService::OWNERSHIP_NONE;

  login_performer_->CompleteLogin(username, password);
  WizardAccessibilityHelper::GetInstance()->MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNING_IN).c_str(),
      false, true);
}

void ExistingUserController::Login(const std::string& username,
                                   const std::string& password) {
  if (username.empty() || password.empty())
    return;
  SetStatusAreaEnabled(false);
  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  // If the device is not owned yet, successfully logged in user will be owner.
  is_owner_login_ = OwnershipService::GetSharedInstance()->GetStatus(true) ==
      OwnershipService::OWNERSHIP_NONE;

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
  SetOwnerUserInCryptohome();

  // Check allow_guest in case this call is fired from key accelerator.
  // Must not proceed without signature verification.
  bool trusted_setting_available = user_settings_->RequestTrustedAllowGuest(
      base::Bind(&ExistingUserController::LoginAsGuest,
                 weak_factory_.GetWeakPtr()));
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

void ExistingUserController::OnStartEnterpriseEnrollment() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDevicePolicy)) {
    ownership_checker_.reset(new OwnershipStatusChecker(
        base::Bind(
            &ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
            base::Unretained(this))));
  }
}

void ExistingUserController::OnEnrollmentOwnershipCheckCompleted(
    OwnershipService::Status status,
    bool current_user_is_owner) {
  if (status == OwnershipService::OWNERSHIP_NONE) {
    host_->StartWizard(WizardController::kEnterpriseEnrollmentScreenName,
                       GURL());
    login_display_->OnFadeOut();
  }
  ownership_checker_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnLoginFailure(const LoginFailure& failure) {
  guest_mode_url_ = GURL::EmptyGURL();
  std::string error = failure.GetErrorString();

  // Check networking after trying to login in case user is
  // cached locally or the local admin account.
  bool is_known_user =
      UserManager::Get()->IsKnownUser(last_login_attempt_username_);
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
  if (!network || !CrosLibrary::Get()->EnsureLoaded()) {
    ShowError(IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY, error);
  } else if (!network->Connected()) {
    if (is_known_user)
      ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
    else
      ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
  } else {
    // Network is connected.
    const Network* active_network = network->active_network();
    if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
        failure.error().state() == GoogleServiceAuthError::CAPTCHA_REQUIRED) {
      if (!failure.error().captcha().image_url.is_empty()) {
        CaptchaView* view =
            new CaptchaView(failure.error().captcha().image_url, false);
        view->Init();
        view->set_delegate(this);
        views::Widget* window = browser::CreateViewsWindow(
            GetNativeWindow(), view);
        window->SetAlwaysOnTop(true);
        window->Show();
      } else {
        LOG(WARNING) << "No captcha image url was found?";
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      }
    } else if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
               failure.error().state() ==
                   GoogleServiceAuthError::HOSTED_NOT_ALLOWED) {
      ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED, error);
    } else if ((active_network && active_network->restricted_pool()) ||
               (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
                failure.error().state() ==
                    GoogleServiceAuthError::SERVICE_UNAVAILABLE)) {
      // Use explicit captive portal state (restricted_pool()) or implicit one.
      // SERVICE_UNAVAILABLE is generated in 2 cases:
      // 1. ClientLogin returns ServiceUnavailable code.
      // 2. Internet connectivity may be behind the captive portal.
      // Suggesting user to try sign in to a portal in Guest mode.
      if (UserCrosSettingsProvider::cached_allow_guest())
        ShowError(IDS_LOGIN_ERROR_CAPTIVE_PORTAL, error);
      else
        ShowError(IDS_LOGIN_ERROR_CAPTIVE_PORTAL_NO_GUEST_MODE, error);
    } else {
      if (!is_known_user)
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_NEW, error);
      else
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
    }
  }

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);
  SetStatusAreaEnabled(true);

  if (login_status_consumer_)
    login_status_consumer_->OnLoginFailure(failure);
}

void ExistingUserController::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests,
    bool using_oauth) {
  bool known_user = UserManager::Get()->IsKnownUser(username);
  bool login_only =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kLoginScreen) == WizardController::kLoginScreenName;
  ready_for_browser_launch_ = known_user || login_only;

  two_factor_credentials_ = credentials.two_factor;

  bool has_cookies =
      login_performer_->auth_mode() == LoginPerformer::AUTH_MODE_EXTENSION;

  // LoginPerformer instance will delete itself once online auth result is OK.
  // In case of failure it'll bring up ScreenLock and ask for
  // correct password/display error message.
  // Even in case when following online,offline protocol and returning
  // requests_pending = false, let LoginPerformer delete itself.
  login_performer_->set_delegate(NULL);
  ignore_result(login_performer_.release());

  // Will call OnProfilePrepared() in the end.
  LoginUtils::Get()->PrepareProfile(username,
                                    password,
                                    credentials,
                                    pending_requests,
                                    using_oauth,
                                    has_cookies,
                                    this);


  // Notifiy LoginDisplay to allow it provide visual feedback to user.
  login_display_->OnLoginSuccess(username);
}

void ExistingUserController::OnProfilePrepared(Profile* profile) {
  // TODO(nkostylev): May add login UI implementation callback call.
  if (!ready_for_browser_launch_) {
    // Add the appropriate first-login URL.
#if !defined(TOUCH_UI)
    PrefService* prefs = g_browser_process->local_state();
    const std::string current_locale =
        StringToLowerASCII(prefs->GetString(prefs::kApplicationLocale));
    std::string start_url;
    if (prefs->GetBoolean(prefs::kAccessibilityEnabled) &&
        current_locale.find("en") != std::string::npos) {
      start_url = kChromeVoxTutorialURL;
    } else {
      const char* url = kGetStartedURLPattern;
      if (is_owner_login_)
        url = kGetStartedOwnerURLPattern;
      start_url = base::StringPrintf(url, current_locale.c_str());
    }
    CommandLine::ForCurrentProcess()->AppendArg(start_url);
#endif

    ServicesCustomizationDocument* customization =
      ServicesCustomizationDocument::GetInstance();
    if (!ServicesCustomizationDocument::WasApplied() &&
        customization->IsReady()) {
      std::string locale = g_browser_process->GetApplicationLocale();
      std::string initial_start_page =
          customization->GetInitialStartPage(locale);
      if (!initial_start_page.empty())
        CommandLine::ForCurrentProcess()->AppendArg(initial_start_page);
      customization->ApplyCustomization();
    }

    if (two_factor_credentials_) {
      // If we have a two factor error and and this is a new user,
      // load the personal settings page.
      // TODO(stevenjb): direct the user to a lightweight sync login page.
      CommandLine::ForCurrentProcess()->AppendArg(kSettingsSyncLoginURL);
    }

#ifndef NDEBUG
    if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeSkipPostLogin)) {
      ready_for_browser_launch_ = true;
      LoginUtils::DoBrowserLaunch(profile, host_);
      host_ = NULL;
    } else {
#endif
      ActivateWizard(WizardController::IsDeviceRegistered() ?
          WizardController::kUserImageScreenName :
          WizardController::kRegistrationScreenName);
#ifndef NDEBUG
    }
#endif
  } else {
    LoginUtils::DoBrowserLaunch(profile, host_);
    // Inform |login_status_consumer_| about successful login after
    // browser launch.  Set most params to empty since they're not needed.
    if (login_status_consumer_)
      login_status_consumer_->OnLoginSuccess(
          "", "", GaiaAuthConsumer::ClientLoginResult(), false, false);
    host_ = NULL;
  }
  login_display_->OnFadeOut();
}

void ExistingUserController::OnOffTheRecordLoginSuccess() {
  if (WizardController::IsDeviceRegistered()) {
    LoginUtils::Get()->CompleteOffTheRecordLogin(guest_mode_url_);
  } else {
    // Postpone CompleteOffTheRecordLogin until registration completion.
    // TODO(nkostylev): Kind of hack. We have to instruct UserManager here
    // that we're actually logged in as Guest user as we'll ask UserManager
    // later in the code path whether we've signed in as Guest and depending
    // on that would either show image screen or call CompleteOffTheRecordLogin.
    UserManager::Get()->GuestUserLoggedIn();
    ActivateWizard(WizardController::kRegistrationScreenName);
  }

  if (login_status_consumer_)
    login_status_consumer_->OnOffTheRecordLoginSuccess();
}

void ExistingUserController::OnPasswordChangeDetected(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Must not proceed without signature verification.
  bool trusted_setting_available = user_settings_->RequestTrustedOwner(
      base::Bind(&ExistingUserController::OnPasswordChangeDetected,
                 weak_factory_.GetWeakPtr(), credentials));
  if (!trusted_setting_available) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  // Passing 'false' here enables "full sync" mode in the dialog,
  // which disables the requirement for the old owner password,
  // allowing us to recover from a lost owner password/homedir.
  // TODO(gspencer): We shouldn't have to erase stateful data when
  // doing this.  See http://crosbug.com/9115 http://crosbug.com/7792
  PasswordChangedView* view = new PasswordChangedView(this, false);
  views::Widget* window = browser::CreateViewsWindow(GetNativeWindow(), view);
  window->SetAlwaysOnTop(true);
  window->Show();

  if (login_status_consumer_)
    login_status_consumer_->OnPasswordChangeDetected(credentials);
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
  GURL start_url;
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest())
    start_url = guest_mode_url_;
  host_->StartWizard(screen_name, start_url);
}

gfx::NativeWindow ExistingUserController::GetNativeWindow() const {
  return host_->GetNativeWindow();
}

void ExistingUserController::SetStatusAreaEnabled(bool enable) {
  if (!host_)
    return;
  host_->SetStatusAreaEnabled(enable);
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

void ExistingUserController::SetOwnerUserInCryptohome() {
  bool trusted_owner_available = user_settings_->RequestTrustedOwner(
      base::Bind(&ExistingUserController::SetOwnerUserInCryptohome,
                 weak_factory_.GetWeakPtr()));
  if (!trusted_owner_available) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }
  if (CrosLibrary::Get()->EnsureLoaded()) {
    CryptohomeLibrary* cryptohomed = CrosLibrary::Get()->GetCryptohomeLibrary();
    cryptohomed->AsyncSetOwnerUser(
        UserCrosSettingsProvider::cached_owner(), NULL);

    // Do not invoke AsyncDoAutomaticFreeDiskSpaceControl(NULL) here
    // so it does not delay the following mount. Cleanup will be
    // started in Cryptohomed by timer.
  }
}

}  // namespace chromeos
