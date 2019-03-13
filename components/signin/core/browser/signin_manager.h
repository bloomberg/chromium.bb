// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking
// which user is signed in. See SigninManagerBase for full description of
// responsibilities. The class defined in this file provides functionality
// required by all platforms except Chrome OS.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_

#include "build/build_config.h"

#if defined(OS_CHROMEOS)

#include "components/signin/core/browser/signin_manager_base.h"

#else

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "net/cookies/canonical_cookie.h"

class GaiaCookieManagerService;
class GoogleServiceAuthError;
class PrefService;

namespace identity {
class IdentityManager;
}  // namespace identity

class SigninManager : public SigninManagerBase,
                      public OAuth2TokenService::Observer {
 public:
  // Used to remove accounts from the token service and the account tracker.
  enum class RemoveAccountsOption {
    // Do not remove accounts.
    kKeepAllAccounts,
    // Remove all the accounts.
    kRemoveAllAccounts,
    // Removes the authenticated account if it is in authentication error.
    kRemoveAuthenticatedAccountIfInError
  };

  // This is used to distinguish URLs belonging to the special web signin flow
  // running in the special signin process from other URLs on the same domain.
  // We do not grant WebUI privilieges / bindings to this process or to URLs of
  // this scheme; enforcement of privileges is handled separately by
  // OneClickSigninHelper.
  static const char kChromeSigninEffectiveSite[];

  SigninManager(SigninClient* client,
                ProfileOAuth2TokenService* token_service,
                AccountTrackerService* account_tracker_service,
                GaiaCookieManagerService* cookie_manager_service,
                signin::AccountConsistencyMethod account_consistency);
  ~SigninManager() override;

  // Returns |manager| as a SigninManager instance. Relies on the fact that on
  // platforms where signin_manager.* is built, all SigninManagerBase instances
  // are actually SigninManager instances.
  static SigninManager* FromSigninManagerBase(SigninManagerBase* manager);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // On mobile and on desktop pre-DICE, this also removes all accounts from
  // Chrome by revoking all refresh tokens.
  // On desktop with DICE enabled, this will remove the authenticated account
  // from Chrome only if it is in authentication error. No other accounts are
  // removed.
  void SignOut(signin_metrics::ProfileSignout signout_source_metric,
               signin_metrics::SignoutDelete signout_delete_metric);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // It removes all accounts from Chrome by revoking all refresh tokens.
  void SignOutAndRemoveAllAccounts(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric);

  // Signs a user out, removing the preference, erasing all keys
  // associated with the authenticated user, and canceling all auth in progress.
  // Does not remove the accounts from the token service.
  void SignOutAndKeepAllAccounts(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric);

  // On platforms where SigninManager is responsible for dealing with
  // invalid username policy updates, we need to check this during
  // initialization and sign the user out.
  void FinalizeInitBeforeLoadingRefreshTokens(
      PrefService* local_state) override;

  void Shutdown() override;

  // Signs a user in. SigninManager assumes that |username| can be used to look
  // up the corresponding account_id and gaia_id for this email.
  void SignIn(const std::string& username);

  // Returns whether sign-in is allowed.
  // TODO(crbug.com/806778): Remove method in super-class.
  bool IsSigninAllowed() const override;

  // Sets whether sign-in is allowed or not.
  void SetSigninAllowed(bool allowed);

 protected:
  // The sign out process which is started by SigninClient::PreSignOut()
  virtual void OnSignoutDecisionReached(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric,
      RemoveAccountsOption remove_option,
      SigninClient::SignoutDecision signout_decision);

 private:
  friend class identity::IdentityManager;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, Prohibited);
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, TestAlternateWildcard);

  // Send all observers |GoogleSigninSucceeded| notifications.
  void FireGoogleSigninSucceeded();

  // Send all observers |GoogleSignedOut| notifications.
  void FireGoogleSignedOut(const AccountInfo& account_info);

  // OAuth2TokenService::Observer:
  void OnRefreshTokensLoaded() override;

  // Called to handle an error from a GAIA auth fetch.  Sets the last error
  // to |error|, sends out a notification of login failure and clears the
  // transient signin data.
  void HandleAuthError(const GoogleServiceAuthError& error);

  // Starts the sign out process.
  void StartSignOut(signin_metrics::ProfileSignout signout_source_metric,
                    signin_metrics::SignoutDelete signout_delete_metric,
                    RemoveAccountsOption remove_option);

  void OnSigninAllowedPrefChanged();
  void OnGoogleServicesUsernamePatternChanged();

  // Returns true if the passed username is allowed by policy.
  bool IsAllowedUsername(const std::string& username) const;

  // Object used to use the token to push a GAIA cookie into the cookie jar.
  GaiaCookieManagerService* cookie_manager_service_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  signin::AccountConsistencyMethod account_consistency_;

  base::WeakPtrFactory<SigninManager> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManager);
};

#endif  // !defined(OS_CHROMEOS)

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_
