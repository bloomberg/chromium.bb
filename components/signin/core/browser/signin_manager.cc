// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/identity_utils.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

SigninManager::SigninManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* cookie_manager_service,
    signin::AccountConsistencyMethod account_consistency)
    : SigninManagerBase(client, token_service, account_tracker_service),
      cookie_manager_service_(cookie_manager_service),
      account_consistency_(account_consistency),
      weak_pointer_factory_(this) {}

SigninManager::~SigninManager() {}

void SigninManager::HandleAuthError(const GoogleServiceAuthError& error) {
  if (observer_ != nullptr) {
    observer_->GoogleSigninFailed(error);
  }
}

void SigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  RemoveAccountsOption remove_option =
      (account_consistency_ == signin::AccountConsistencyMethod::kDice)
          ? RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError
          : RemoveAccountsOption::kRemoveAllAccounts;
  StartSignOut(signout_source_metric, signout_delete_metric, remove_option);
}

void SigninManager::SignOutAndRemoveAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kRemoveAllAccounts);
}

void SigninManager::SignOutAndKeepAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kKeepAllAccounts);
}

void SigninManager::StartSignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option) {
  signin_client()->PreSignOut(
      base::BindOnce(&SigninManager::OnSignoutDecisionReached,
                     base::Unretained(this), signout_source_metric,
                     signout_delete_metric, remove_option),
      signout_source_metric);
}

void SigninManager::OnSignoutDecisionReached(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option,
    SigninClient::SignoutDecision signout_decision) {
  DCHECK(IsInitialized());

  signin_metrics::LogSignout(signout_source_metric, signout_delete_metric);
  if (!IsAuthenticated()) {
    return;
  }

  // TODO(crbug.com/887756): Consider moving this higher up, or document why
  // the above blocks are exempt from the |signout_decision| early return.
  if (signout_decision == SigninClient::SignoutDecision::DISALLOW_SIGNOUT) {
    DVLOG(1) << "Ignoring attempt to sign out while signout disallowed";
    return;
  }

  AccountInfo account_info = GetAuthenticatedAccountInfo();
  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = account_info.email;
  const base::Time signin_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          signin_client()->GetPrefs()->GetInt64(prefs::kSignedInTime)));
  ClearAuthenticatedAccountId();
  signin_client()->GetPrefs()->ClearPref(prefs::kGoogleServicesHostedDomain);
  signin_client()->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);
  signin_client()->GetPrefs()->ClearPref(prefs::kGoogleServicesUserAccountId);
  signin_client()->GetPrefs()->ClearPref(prefs::kSignedInTime);

  // Determine the duration the user was logged in and log that to UMA.
  if (!signin_time.is_null()) {
    base::TimeDelta signed_in_duration = base::Time::Now() - signin_time;
    UMA_HISTOGRAM_COUNTS_1M("Signin.SignedInDurationBeforeSignout",
                            signed_in_duration.InMinutes());
  }

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  switch (remove_option) {
    case RemoveAccountsOption::kRemoveAllAccounts:
      VLOG(0) << "Revoking all refresh tokens on server. Reason: sign out, "
              << "IsSigninAllowed: " << IsSigninAllowed();
      token_service()->RevokeAllCredentials(
          signin_metrics::SourceForRefreshTokenOperation::
              kSigninManager_ClearPrimaryAccount);
      break;
    case RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError:
      if (token_service()->RefreshTokenHasError(account_id))
        token_service()->RevokeCredentials(
            account_id, signin_metrics::SourceForRefreshTokenOperation::
                            kSigninManager_ClearPrimaryAccount);
      break;
    case RemoveAccountsOption::kKeepAllAccounts:
      // Do nothing.
      break;
  }

  FireGoogleSignedOut(account_info);
}

void SigninManager::FinalizeInitBeforeLoadingRefreshTokens(
    PrefService* local_state) {
  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(
        prefs::kGoogleServicesUsernamePattern,
        base::Bind(&SigninManager::OnGoogleServicesUsernamePatternChanged,
                   weak_pointer_factory_.GetWeakPtr()));
  }
  signin_allowed_.Init(prefs::kSigninAllowed, signin_client()->GetPrefs(),
                       base::Bind(&SigninManager::OnSigninAllowedPrefChanged,
                                  base::Unretained(this)));

  AccountInfo account_info = GetAuthenticatedAccountInfo();
  if (!account_info.account_id.empty() &&
      (!IsAllowedUsername(account_info.email) || !IsSigninAllowed())) {
    // User is signed in, but the username is invalid or signin is no longer
    // allowed, so the user must be sign out.
    //
    // This may happen in the following cases:
    //   a. The user has toggled off signin allowed in settings.
    //   b. The administrator changed the policy since the last signin.
    //
    // Note: The token service has not yet loaded its credentials, so accounts
    // cannot be revoked here.
    //
    // On desktop, when SigninManager is initializing, the profile was not yet
    // marked with sign out allowed. Therefore sign out is not allowed and all
    // calls to SignOut methods are no-op.
    //
    // TODO(msarda): SignOut methods do not gurantee that sign out can actually
    // be done (this depends on whether sign out is allowed). Add a check here
    // on desktop to make it clear that SignOut does not do anything.
    SignOutAndKeepAllAccounts(signin_metrics::SIGNIN_PREF_CHANGED_DURING_SIGNIN,
                              signin_metrics::SignoutDelete::IGNORE_METRIC);
  }

  // It is important to only load credentials after starting to observe the
  // token service.
  token_service()->AddObserver(this);
}

void SigninManager::Shutdown() {
  token_service()->RemoveObserver(this);
  local_state_pref_registrar_.RemoveAll();
  SigninManagerBase::Shutdown();
}

void SigninManager::OnGoogleServicesUsernamePatternChanged() {
  if (IsAuthenticated() &&
      !IsAllowedUsername(GetAuthenticatedAccountInfo().email)) {
    // Signed in user is invalid according to the current policy so sign
    // the user out.
    SignOut(signin_metrics::GOOGLE_SERVICE_NAME_PATTERN_CHANGED,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
}

bool SigninManager::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

void SigninManager::SetSigninAllowed(bool allowed) {
  signin_allowed_.SetValue(allowed);
}

void SigninManager::OnSigninAllowedPrefChanged() {
  if (!IsSigninAllowed() && IsAuthenticated())
    SignOut(signin_metrics::SIGNOUT_PREF_CHANGED,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
}

// static
SigninManager* SigninManager::FromSigninManagerBase(
    SigninManagerBase* manager) {
  return static_cast<SigninManager*>(manager);
}

bool SigninManager::IsAllowedUsername(const std::string& username) const {
  const PrefService* local_state = local_state_pref_registrar_.prefs();
  if (!local_state)
    return true;  // In a unit test with no local state - all names are allowed.

  std::string pattern =
      local_state->GetString(prefs::kGoogleServicesUsernamePattern);
  return identity::IsUsernameAllowedByPattern(username, pattern);
}

void SigninManager::OnExternalSigninCompleted(const std::string& username) {
  AccountInfo info =
      account_tracker_service()->FindAccountInfoByEmail(username);
  DCHECK(!info.gaia.empty());
  DCHECK(!info.email.empty());

  bool reauth_in_progress = IsAuthenticated();

  signin_client()->GetPrefs()->SetInt64(
      prefs::kSignedInTime,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  SetAuthenticatedAccountInfo(info.gaia, info.email);

  if (!reauth_in_progress)
    FireGoogleSigninSucceeded();

  signin_metrics::LogSigninProfile(signin_client()->IsFirstRun(),
                                   signin_client()->GetInstallDate());
}

void SigninManager::FireGoogleSigninSucceeded() {
  const AccountInfo account_info = GetAuthenticatedAccountInfo();
  if (observer_ != nullptr) {
    observer_->GoogleSigninSucceeded(account_info);
  }
}

void SigninManager::FireGoogleSignedOut(const AccountInfo& account_info) {
  if (observer_ != nullptr) {
    observer_->GoogleSignedOut(account_info);
  }
}

void SigninManager::OnRefreshTokensLoaded() {
  token_service()->RemoveObserver(this);

  if (account_tracker_service()->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    account_tracker_service()->SetMigrationDone();
  }

  // Remove account information from the account tracker service if needed.
  if (token_service()->HasLoadCredentialsFinishedWithNoErrors()) {
    std::vector<AccountInfo> accounts_in_tracker_service =
        account_tracker_service()->GetAccounts();
    for (const auto& account : accounts_in_tracker_service) {
      if (GetAuthenticatedAccountId() != account.account_id &&
          !token_service()->RefreshTokenIsAvailable(account.account_id)) {
        DVLOG(0) << "Removed account from account tracker service: "
                 << account.account_id;
        account_tracker_service()->RemoveAccount(account.account_id);
      }
    }
  }
}
