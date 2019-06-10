// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/primary_account_policy_manager.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/identity_utils.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

PrimaryAccountPolicyManager::PrimaryAccountPolicyManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    signin::AccountConsistencyMethod account_consistency)
    : PrimaryAccountManager(client,
                            token_service,
                            account_tracker_service,
                            account_consistency),
      weak_pointer_factory_(this) {}

PrimaryAccountPolicyManager::~PrimaryAccountPolicyManager() {
  local_state_pref_registrar_.RemoveAll();
}

void PrimaryAccountPolicyManager::FinalizeInitBeforeLoadingRefreshTokens(
    PrefService* local_state) {
  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(
        prefs::kGoogleServicesUsernamePattern,
        base::Bind(&PrimaryAccountPolicyManager::
                       OnGoogleServicesUsernamePatternChanged,
                   weak_pointer_factory_.GetWeakPtr()));
  }
  signin_allowed_.Init(
      prefs::kSigninAllowed, signin_client()->GetPrefs(),
      base::Bind(&PrimaryAccountPolicyManager::OnSigninAllowedPrefChanged,
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
    // On desktop, when PrimaryAccountManager is initializing, the profile was
    // not yet marked with sign out allowed. Therefore sign out is not allowed
    // and all calls to SignOut methods are no-op.
    //
    // TODO(msarda): SignOut methods do not guarantee that sign out can actually
    // be done (this depends on whether sign out is allowed). Add a check here
    // on desktop to make it clear that SignOut does not do anything.
    SignOutAndKeepAllAccounts(signin_metrics::SIGNIN_PREF_CHANGED_DURING_SIGNIN,
                              signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
}

void PrimaryAccountPolicyManager::OnGoogleServicesUsernamePatternChanged() {
  if (IsAuthenticated() &&
      !IsAllowedUsername(GetAuthenticatedAccountInfo().email)) {
    // Signed in user is invalid according to the current policy so sign
    // the user out.
    SignOut(signin_metrics::GOOGLE_SERVICE_NAME_PATTERN_CHANGED,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
}

bool PrimaryAccountPolicyManager::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

void PrimaryAccountPolicyManager::OnSigninAllowedPrefChanged() {
  if (!IsSigninAllowed() && IsAuthenticated()) {
    VLOG(0) << "IsSigninAllowed() set to false, signing out the user";
    SignOut(signin_metrics::SIGNOUT_PREF_CHANGED,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
}

bool PrimaryAccountPolicyManager::IsAllowedUsername(
    const std::string& username) const {
  const PrefService* local_state = local_state_pref_registrar_.prefs();
  if (!local_state)
    return true;  // In a unit test with no local state - all names are allowed.

  std::string pattern =
      local_state->GetString(prefs::kGoogleServicesUsernamePattern);
  return identity::IsUsernameAllowedByPattern(username, pattern);
}
