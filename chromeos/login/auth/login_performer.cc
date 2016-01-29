// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/login_performer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/user_names.h"
#include "chromeos/login_event_recorder.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/signin/core/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using base::UserMetricsAction;

namespace chromeos {

LoginPerformer::LoginPerformer(scoped_refptr<base::TaskRunner> task_runner,
                               Delegate* delegate)
    : delegate_(delegate),
      task_runner_(task_runner),
      last_login_failure_(AuthFailure::AuthFailureNone()),
      password_changed_(false),
      password_changed_callback_count_(0),
      auth_mode_(AUTH_MODE_INTERNAL),
      weak_factory_(this) {}

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
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::RecordAction(UserMetricsAction("Login_Failure"));

  UMA_HISTOGRAM_ENUMERATION("Login.FailureReason",
                            failure.reason(),
                            AuthFailure::NUM_FAILURE_REASONS);

  DVLOG(1) << "failure.reason " << failure.reason();
  DVLOG(1) << "failure.error.state " << failure.error().state();

  last_login_failure_ = failure;
  if (delegate_) {
    delegate_->SetAuthFlowOffline(user_context_.GetAuthFlow() ==
                                  UserContext::AUTH_FLOW_OFFLINE);
    delegate_->OnAuthFailure(failure);
    return;
  } else {
    // COULD_NOT_MOUNT_CRYPTOHOME, COULD_NOT_MOUNT_TMPFS:
    // happens during offline auth only.
    NOTREACHED();
  }
}

void LoginPerformer::OnAuthSuccess(const UserContext& user_context) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::RecordAction(UserMetricsAction("Login_Success"));
  VLOG(1) << "LoginSuccess hash: " << user_context.GetUserIDHash();
  DCHECK(delegate_);
  // After delegate_->OnAuthSuccess(...) is called, delegate_ releases
  // LoginPerformer ownership. LP now manages it's lifetime on its own.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  delegate_->OnAuthSuccess(user_context);
}

void LoginPerformer::OnOffTheRecordAuthSuccess() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::RecordAction(UserMetricsAction("Login_GuestLoginSuccess"));

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

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, public:

void LoginPerformer::NotifyWhitelistCheckFailure() {
  if (delegate_)
    delegate_->WhiteListCheckFailed(
        user_context_.GetAccountId().GetUserEmail());
  else
    NOTREACHED();
}

void LoginPerformer::PerformLogin(const UserContext& user_context,
                                  AuthorizationMode auth_mode) {
  auth_mode_ = auth_mode;
  user_context_ = user_context;

  if (RunTrustedCheck(base::Bind(&LoginPerformer::DoPerformLogin,
                                 weak_factory_.GetWeakPtr(),
                                 user_context_,
                                 auth_mode))) {
    return;
  }
  DoPerformLogin(user_context_, auth_mode);
}

void LoginPerformer::DoPerformLogin(const UserContext& user_context,
                                    AuthorizationMode auth_mode) {
  bool wildcard_match = false;

  const AccountId& account_id = user_context.GetAccountId();
  if (!IsUserWhitelisted(account_id, &wildcard_match)) {
    NotifyWhitelistCheckFailure();
    return;
  }

  if (user_context.GetAuthFlow() == UserContext::AUTH_FLOW_EASY_UNLOCK)
    SetupEasyUnlockUserFlow(user_context.GetAccountId());

  switch (auth_mode_) {
    case AUTH_MODE_EXTENSION: {
      RunOnlineWhitelistCheck(
          account_id, wildcard_match, user_context.GetRefreshToken(),
          base::Bind(&LoginPerformer::StartLoginCompletion,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&LoginPerformer::NotifyWhitelistCheckFailure,
                     weak_factory_.GetWeakPtr()));
      break;
    }
    case AUTH_MODE_INTERNAL:
      StartAuthentication();
      break;
  }
}

void LoginPerformer::LoginAsSupervisedUser(const UserContext& user_context) {
  DCHECK_EQ(
      chromeos::login::kSupervisedUserDomain,
      gaia::ExtractDomainName(user_context.GetAccountId().GetUserEmail()));

  user_context_ = user_context;
  user_context_.SetUserType(user_manager::USER_TYPE_SUPERVISED);

  if (RunTrustedCheck(base::Bind(&LoginPerformer::TrustedLoginAsSupervisedUser,
                                 weak_factory_.GetWeakPtr(),
                                 user_context_))) {
    return;
  }
  TrustedLoginAsSupervisedUser(user_context_);
}

void LoginPerformer::TrustedLoginAsSupervisedUser(
    const UserContext& user_context) {
  if (!AreSupervisedUsersAllowed()) {
    LOG(ERROR) << "Login attempt of supervised user detected.";
    delegate_->WhiteListCheckFailed(user_context.GetAccountId().GetUserEmail());
    return;
  }

  SetupSupervisedUserFlow(user_context.GetAccountId());
  UserContext user_context_copy = TransformSupervisedKey(user_context);

  if (UseExtendedAuthenticatorForSupervisedUser(user_context)) {
    EnsureExtendedAuthenticator();
    // TODO(antrim) : Replace empty callback with explicit method.
    // http://crbug.com/351268
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ExtendedAuthenticator::AuthenticateToMount,
                   extended_authenticator_.get(),
                   user_context_copy,
                   ExtendedAuthenticator::ResultCallback()));

  } else {
    EnsureAuthenticator();
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&Authenticator::LoginAsSupervisedUser,
                                      authenticator_.get(),
                                      user_context_copy));
  }
}

void LoginPerformer::LoginAsPublicSession(const UserContext& user_context) {
  if (!CheckPolicyForUser(user_context.GetAccountId())) {
    DCHECK(delegate_);
    if (delegate_)
      delegate_->PolicyLoadFailed();
    return;
  }

  EnsureAuthenticator();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Authenticator::LoginAsPublicSession,
                                    authenticator_.get(),
                                    user_context));
}

void LoginPerformer::LoginOffTheRecord() {
  EnsureAuthenticator();
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Authenticator::LoginOffTheRecord, authenticator_.get()));
}

void LoginPerformer::LoginAsKioskAccount(const std::string& app_user_id,
                                         bool use_guest_mount) {
  EnsureAuthenticator();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Authenticator::LoginAsKioskAccount,
                                    authenticator_.get(),
                                    app_user_id,
                                    use_guest_mount));
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Authenticator::RecoverEncryptedData,
                                    authenticator_.get(),
                                    old_password));
}

void LoginPerformer::ResyncEncryptedData() {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Authenticator::ResyncEncryptedData, authenticator_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::EnsureExtendedAuthenticator() {
  if (extended_authenticator_.get())
    extended_authenticator_->SetConsumer(NULL);
  extended_authenticator_ = ExtendedAuthenticator::Create(this);
}

void LoginPerformer::StartLoginCompletion() {
  DVLOG(1) << "Login completion started";
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  content::BrowserContext* browser_context = GetSigninContext();
  EnsureAuthenticator();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&chromeos::Authenticator::CompleteLogin,
                                    authenticator_.get(),
                                    browser_context,
                                    user_context_));
  user_context_.ClearSecrets();
}

void LoginPerformer::StartAuthentication() {
  DVLOG(1) << "Auth started";
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  if (delegate_) {
    EnsureAuthenticator();
    content::BrowserContext* browser_context = GetSigninContext();
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&Authenticator::AuthenticateToLogin,
                                      authenticator_.get(),
                                      base::Unretained(browser_context),
                                      user_context_));
  } else {
    NOTREACHED();
  }
  user_context_.ClearSecrets();
}

void LoginPerformer::EnsureAuthenticator() {
  authenticator_ = CreateAuthenticator();
}
}  // namespace chromeos
