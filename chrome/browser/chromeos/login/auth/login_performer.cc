// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/login_performer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_constants.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_login_flow.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/user_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using base::UserMetricsAction;
using content::BrowserThread;

namespace chromeos {

LoginPerformer::LoginPerformer(Delegate* delegate)
    : online_attempt_host_(this),
      last_login_failure_(AuthFailure::AuthFailureNone()),
      delegate_(delegate),
      password_changed_(false),
      password_changed_callback_count_(0),
      auth_mode_(AUTH_MODE_INTERNAL),
      weak_factory_(this) {
}

LoginPerformer::~LoginPerformer() {
  DVLOG(1) << "Deleting LoginPerformer";
  if (authenticator_.get())
    authenticator_->SetConsumer(NULL);
  if (extended_authenticator_.get())
    extended_authenticator_->SetConsumer(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, AuthStatusConsumer implementation:

void LoginPerformer::OnAuthFailure(const AuthFailure& failure) {
  content::RecordAction(UserMetricsAction("Login_Failure"));
  UMA_HISTOGRAM_ENUMERATION("Login.FailureReason",
                            failure.reason(),
                            AuthFailure::NUM_FAILURE_REASONS);

  DVLOG(1) << "failure.reason " << failure.reason();
  DVLOG(1) << "failure.error.state " << failure.error().state();

  last_login_failure_ = failure;
  if (delegate_) {
    delegate_->OnAuthFailure(failure);
    return;
  } else {
    // COULD_NOT_MOUNT_CRYPTOHOME, COULD_NOT_MOUNT_TMPFS:
    // happens during offline auth only.
    NOTREACHED();
  }
}

void LoginPerformer::OnRetailModeAuthSuccess(const UserContext& user_context) {
  content::RecordAction(
      UserMetricsAction("Login_DemoUserLoginSuccess"));
  AuthStatusConsumer::OnRetailModeAuthSuccess(user_context);
}

void LoginPerformer::OnAuthSuccess(const UserContext& user_context) {
  content::RecordAction(UserMetricsAction("Login_Success"));
  VLOG(1) << "LoginSuccess hash: " << user_context.GetUserIDHash();
  DCHECK(delegate_);
  // After delegate_->OnAuthSuccess(...) is called, delegate_ releases
  // LoginPerformer ownership. LP now manages it's lifetime on its own.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  delegate_->OnAuthSuccess(user_context);
}

void LoginPerformer::OnOffTheRecordAuthSuccess() {
  content::RecordAction(
      UserMetricsAction("Login_GuestLoginSuccess"));

  if (delegate_)
    delegate_->OnOffTheRecordAuthSuccess();
  else
    NOTREACHED();
}

void LoginPerformer::OnPasswordChangeDetected() {
  password_changed_ = true;
  password_changed_callback_count_++;
  if (delegate_) {
    delegate_->OnPasswordChangeDetected();
  } else {
    NOTREACHED();
  }
}

void LoginPerformer::OnChecked(const std::string& username, bool success) {
  if (!delegate_) {
    // Delegate is reset in case of successful offline login.
    // See ExistingUserConstoller::OnAuthSuccess().
    // Case when user has changed password and enters old password
    // does not block user from sign in yet.
    return;
  }
  delegate_->OnOnlineChecked(username, success);
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, public:

void LoginPerformer::PerformLogin(const UserContext& user_context,
                                  AuthorizationMode auth_mode) {
  auth_mode_ = auth_mode;
  user_context_ = user_context;

  CrosSettings* cros_settings = CrosSettings::Get();

  // Whitelist check is always performed during initial login.
  CrosSettingsProvider::TrustedStatus status =
      cros_settings->PrepareTrustedValues(
          base::Bind(&LoginPerformer::PerformLogin,
                     weak_factory_.GetWeakPtr(),
                     user_context_, auth_mode));
  // Must not proceed without signature verification.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    if (delegate_)
      delegate_->PolicyLoadFailed();
    else
      NOTREACHED();
    return;
  } else if (status != CrosSettingsProvider::TRUSTED) {
    // Value of AllowNewUser setting is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  bool wildcard_match = false;
  std::string email = gaia::CanonicalizeEmail(user_context.GetUserID());
  bool is_whitelisted = LoginUtils::IsWhitelisted(email, &wildcard_match);
  if (is_whitelisted) {
    switch (auth_mode_) {
      case AUTH_MODE_EXTENSION: {
        // On enterprise devices, reconfirm login permission with the server.
        policy::BrowserPolicyConnectorChromeOS* connector =
            g_browser_process->platform_part()
                ->browser_policy_connector_chromeos();
        if (connector->IsEnterpriseManaged() && wildcard_match &&
            !connector->IsNonEnterpriseUser(email)) {
          wildcard_login_checker_.reset(new policy::WildcardLoginChecker());
          wildcard_login_checker_->Start(
                  ProfileHelper::GetSigninProfile()->GetRequestContext(),
                  base::Bind(&LoginPerformer::OnlineWildcardLoginCheckCompleted,
                             weak_factory_.GetWeakPtr()));
        } else {
          StartLoginCompletion();
        }
        break;
      }
      case AUTH_MODE_INTERNAL:
        StartAuthentication();
        break;
    }
  } else {
    if (delegate_)
      delegate_->WhiteListCheckFailed(user_context.GetUserID());
    else
      NOTREACHED();
  }
}

void LoginPerformer::LoginAsSupervisedUser(
    const UserContext& user_context) {
  DCHECK_EQ(chromeos::login::kSupervisedUserDomain,
            gaia::ExtractDomainName(user_context.GetUserID()));

  CrosSettings* cros_settings = CrosSettings::Get();
  CrosSettingsProvider::TrustedStatus status =
        cros_settings->PrepareTrustedValues(
            base::Bind(&LoginPerformer::LoginAsSupervisedUser,
                       weak_factory_.GetWeakPtr(),
                       user_context_));
  // Must not proceed without signature verification.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    if (delegate_)
      delegate_->PolicyLoadFailed();
    else
      NOTREACHED();
    return;
  } else if (status != CrosSettingsProvider::TRUSTED) {
    // Value of kAccountsPrefSupervisedUsersEnabled setting is still not
    // verified. Another attempt will be invoked after verification completion.
    return;
  }

  if (!UserManager::Get()->AreSupervisedUsersAllowed()) {
    LOG(ERROR) << "Login attempt of supervised user detected.";
    delegate_->WhiteListCheckFailed(user_context.GetUserID());
    return;
  }

  SupervisedUserLoginFlow* new_flow =
      new SupervisedUserLoginFlow(user_context.GetUserID());
  new_flow->set_host(
      UserManager::Get()->GetUserFlow(user_context.GetUserID())->host());
  UserManager::Get()->SetUserFlow(user_context.GetUserID(), new_flow);

  SupervisedUserAuthentication* authentication = UserManager::Get()->
      GetSupervisedUserManager()->GetAuthentication();

  UserContext user_context_copy = authentication->TransformKey(user_context);

  if (authentication->GetPasswordSchema(user_context.GetUserID()) ==
      SupervisedUserAuthentication::SCHEMA_SALT_HASHED) {
    if (extended_authenticator_.get()) {
      extended_authenticator_->SetConsumer(NULL);
    }
    extended_authenticator_ = new ExtendedAuthenticator(this);
    // TODO(antrim) : Replace empty callback with explicit method.
    // http://crbug.com/351268
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ExtendedAuthenticator::AuthenticateToMount,
                   extended_authenticator_.get(),
                   user_context_copy,
                   ExtendedAuthenticator::ResultCallback()));

  } else {
    authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&Authenticator::LoginAsSupervisedUser,
                   authenticator_.get(),
                   user_context_copy));
  }
}

void LoginPerformer::LoginRetailMode() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginRetailMode, authenticator_.get()));
}

void LoginPerformer::LoginOffTheRecord() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginOffTheRecord, authenticator_.get()));
}

void LoginPerformer::LoginAsPublicSession(const UserContext& user_context) {
  // Login is not allowed if policy could not be loaded for the account.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceLocalAccountPolicyService* policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!policy_service ||
      !policy_service->IsPolicyAvailableForUser(user_context.GetUserID())) {
    DCHECK(delegate_);
    if (delegate_)
      delegate_->PolicyLoadFailed();
    return;
  }

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginAsPublicSession,
                 authenticator_.get(),
                 user_context));
}

void LoginPerformer::LoginAsKioskAccount(const std::string& app_user_id,
                                         bool use_guest_mount) {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginAsKioskAccount, authenticator_.get(),
                 app_user_id, use_guest_mount));
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::RecoverEncryptedData, authenticator_.get(),
                 old_password));
}

void LoginPerformer::ResyncEncryptedData() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::ResyncEncryptedData, authenticator_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::StartLoginCompletion() {
  DVLOG(1) << "Login completion started";
  BootTimesLoader::Get()->AddLoginTimeMarker("AuthStarted", false);
  Profile* profile = ProfileHelper::GetSigninProfile();

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::CompleteLogin, authenticator_.get(),
                 profile,
                 user_context_));
  user_context_.ClearSecrets();
}

void LoginPerformer::StartAuthentication() {
  DVLOG(1) << "Auth started";
  BootTimesLoader::Get()->AddLoginTimeMarker("AuthStarted", false);
  Profile* profile = ProfileHelper::GetSigninProfile();
  if (delegate_) {
    authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Authenticator::AuthenticateToLogin, authenticator_.get(),
                   profile,
                   user_context_));
    // Make unobtrusive online check. It helps to determine password change
    // state in the case when offline login fails.
    online_attempt_host_.Check(profile, user_context_);
  } else {
    NOTREACHED();
  }
  user_context_.ClearSecrets();
}

void LoginPerformer::OnlineWildcardLoginCheckCompleted(
    policy::WildcardLoginChecker::Result result) {
  if (result == policy::WildcardLoginChecker::RESULT_ALLOWED) {
    StartLoginCompletion();
  } else {
    if (delegate_)
      delegate_->WhiteListCheckFailed(user_context_.GetUserID());
  }
}

}  // namespace chromeos
