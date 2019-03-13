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
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/core/browser/account_info.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountTrackerService;
class PrefRegistrySimple;
class PrefService;
class ProfileOAuth2TokenService;
class SigninClient;

class SigninManagerBase : public KeyedService {
 public:
  class Observer {
   public:
    // Called when a user fails to sign into Google services such as sync.
    virtual void GoogleSigninFailed(const GoogleServiceAuthError& error) {}

    // Called when a user signs into Google services such as sync.
    // This method is not called during a reauth.
    virtual void GoogleSigninSucceeded(const AccountInfo& account_info) {}

    // Called when the currently signed-in user for a user has been signed out.
    virtual void GoogleSignedOut(const AccountInfo& account_info) {}

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
                    AccountTrackerService* account_tracker_service);
#if !defined(OS_CHROMEOS)
 public:
#endif

  ~SigninManagerBase() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers per-install prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // If user was signed in, load tokens from DB if available.
  void Initialize(PrefService* local_state);
  bool IsInitialized() const;

  // Returns true if a signin to Chrome is allowed (by policy or pref).
  // TODO(crbug.com/806778): this method should not be used externally,
  // instead the value of the kSigninAllowed preference should be checked.
  // Once all external code has been modified, this method will be removed.
  virtual bool IsSigninAllowed() const;

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

  base::WeakPtrFactory<SigninManagerBase> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerBase);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_BASE_H_
