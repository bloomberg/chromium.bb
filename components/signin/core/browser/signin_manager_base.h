// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking
// which user is signed in.
//
// **NOTE** on semantics of SigninManager:
//
// Once a signin is successful, the username becomes "established" and will not
// be cleared until a SignOut operation is performed (persists across
// restarts). Until that happens, the signin manager can still be used to
// refresh credentials, but changing the username is not permitted.
//
// On Chrome OS, because of the existence of other components that handle login
// and signin at a higher level, all that is needed from a SigninManager is
// caching / handling of the "authenticated username" field, and TokenService
// initialization, so that components that depend on these two things
// (i.e on desktop) can continue using it / don't need to change. For this
// reason, SigninManagerBase is all that exists on Chrome OS. For desktop,
// see signin/signin_manager.h.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_BASE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_BASE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountTrackerService;
class GaiaCookieManagerService;
class PrefRegistrySimple;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;

class SigninManagerBase {
 public:
  class Observer {
   public:
    // Called when a user signs into Google services such as sync.
    // This method is not called during a reauth.
    virtual void GoogleSigninSucceeded(const AccountInfo& account_info) {}

    // Called when the currently signed-in user for a user has been signed out.
    virtual void GoogleSignedOut(const AccountInfo& account_info) {}

    // Called during the signin as soon as
    // SigninManagerBase::authenticated_account_id_ is set.
    virtual void AuthenticatedAccountSet(const AccountInfo& account_info) {}

    // Called during the signout as soon as
    // SigninManagerBase::authenticated_account_id_ is cleared.
    virtual void AuthenticatedAccountCleared() {}

   protected:
    virtual ~Observer() {}

   private:
    // SigninManagers that fire notifications.
    friend class SigninManager;
  };

// On non-ChromeOS platforms, SigninManagerBase should only be instantiated
// via the derived SigninManager class, as the codewise assumes the
// invariant that any SigninManagerBase object can be cast to a
// SigninManager object when not on ChromeOS. Make the constructor private
// and add SigninManager as a friend to support this.
// TODO(883648): Eliminate the need to downcast SigninManagerBase to
// SigninManager and then eliminate this as well.
#if !defined(OS_CHROMEOS)
 private:
#endif
  SigninManagerBase(SigninClient* client,
                    ProfileOAuth2TokenService* token_service,
                    AccountTrackerService* account_tracker_service,
                    GaiaCookieManagerService* cookie_manager_service,
                    signin::AccountConsistencyMethod account_consistency);
#if !defined(OS_CHROMEOS)
 public:
#endif

#if !defined(OS_CHROMEOS)
  // Used to remove accounts from the token service and the account tracker.
  enum class RemoveAccountsOption {
    // Do not remove accounts.
    kKeepAllAccounts,
    // Remove all the accounts.
    kRemoveAllAccounts,
    // Removes the authenticated account if it is in authentication error.
    kRemoveAuthenticatedAccountIfInError
  };
#endif

  virtual ~SigninManagerBase();

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers per-install prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // If user was signed in, load tokens from DB if available.
  void Initialize(PrefService* local_state);
  bool IsInitialized() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the know information of the account. Otherwise, it returns an empty struct.
  AccountInfo GetAuthenticatedAccountInfo() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the account id. Otherwise, it returns an empty string.  This id is the
  // G+/Focus obfuscated gaia id of the user. It can be used to uniquely
  // identify an account, so for example as a key to map accounts to data. For
  // code that needs a unique id to represent the connected account, call this
  // method. Example: the AccountStatusMap type in
  // MutableProfileOAuth2TokenService. For code that needs to know the
  // normalized email address of the connected account, use
  // GetAuthenticatedAccountInfo().email.  Example: to show the string "Signed
  // in as XXX" in the hotdog menu.
  const std::string& GetAuthenticatedAccountId() const;

  // Sets the authenticated user's Gaia ID and display email.  Internally,
  // this will seed the account information in AccountTrackerService and pick
  // the right account_id for this account.
  void SetAuthenticatedAccountInfo(const std::string& gaia_id,
                                   const std::string& email);

  // Returns true if there is an authenticated user.
  bool IsAuthenticated() const;

  // Methods to set or clear the observer of signin.
  // In practice these should only be used by IdentityManager.
  // NOTE: If SetObserver is called, ClearObserver should be called before
  // the destruction of SigninManagerBase.
  void SetObserver(Observer* observer);
  void ClearObserver();

  // Signout API surfaces (not supported on ChromeOS, where signout is not
  // permitted).
#if !defined(OS_CHROMEOS)
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
#endif

 protected:
  SigninClient* signin_client() const { return client_; }

  ProfileOAuth2TokenService* token_service() const { return token_service_; }

  AccountTrackerService* account_tracker_service() const {
    return account_tracker_service_;
  }

  // Invoked at the end of |Initialize| before the refresh token for the primary
  // account is loaded.
  virtual void FinalizeInitBeforeLoadingRefreshTokens(PrefService* local_state);

  // Sets the authenticated user's account id.
  // If the user is already authenticated with the same account id, then this
  // method is a no-op.
  // It is forbidden to call this method if the user is already authenticated
  // with a different account (this method will DCHECK in that case).
  // |account_id| must not be empty. To log the user out, use
  // ClearAuthenticatedAccountId() instead.
  void SetAuthenticatedAccountId(const std::string& account_id);

  // Clears the authenticated user's account id.
  // This method is not public because SigninManagerBase does not allow signing
  // out by default. Subclasses implementing a sign-out functionality need to
  // call this.
  void ClearAuthenticatedAccountId();

  // Observer to notify on signin events.
  // There is a DCHECK on destruction that this has been cleared.
  Observer* observer_ = nullptr;

 private:
  // Added only to allow SigninManager to call the SigninManagerBase
  // constructor while disallowing any ad-hoc subclassing of
  // SigninManagerBase.
  friend class SigninManager;

#if !defined(OS_CHROMEOS)
  // Starts the sign out process.
  void StartSignOut(signin_metrics::ProfileSignout signout_source_metric,
                    signin_metrics::SignoutDelete signout_delete_metric,
                    RemoveAccountsOption remove_option);

  // The sign out process which is started by SigninClient::PreSignOut()
  void OnSignoutDecisionReached(
      signin_metrics::ProfileSignout signout_source_metric,
      signin_metrics::SignoutDelete signout_delete_metric,
      RemoveAccountsOption remove_option,
      SigninClient::SignoutDecision signout_decision);

  // Send all observers |GoogleSignedOut| notifications.
  void FireGoogleSignedOut(const AccountInfo& account_info);
#endif

  SigninClient* client_;

  // The ProfileOAuth2TokenService instance associated with this object. Must
  // outlive this object.
  ProfileOAuth2TokenService* token_service_;

  AccountTrackerService* account_tracker_service_;

  bool initialized_;

  // Account id after successful authentication.
  std::string authenticated_account_id_;

  // The list of callbacks notified on shutdown.
  base::CallbackList<void()> on_shutdown_callback_list_;

  signin::AccountConsistencyMethod account_consistency_;

  base::WeakPtrFactory<SigninManagerBase> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerBase);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_BASE_H_
