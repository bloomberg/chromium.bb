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
#include "components/signin/core/browser/signin_internals_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountTrackerService;
class PrefRegistrySimple;
class PrefService;
class SigninClient;
class SigninErrorController;

class SigninManagerBase : public KeyedService {
 public:
  class Observer {
   public:
    // Called when a user fails to sign into Google services such as sync.
    virtual void GoogleSigninFailed(const GoogleServiceAuthError& error) {}

    // Called when a user signs into Google services such as sync.
    // This method is not called during a reauth.
    virtual void GoogleSigninSucceeded(const AccountInfo& account_info) {}

    // Called when a user signs into Google services such as sync. Also passes
    // the password of the Google account that was used to sign in.
    // This method is not called during a reauth.
    //
    // Observers should override |GoogleSigninSucceeded| if they are not
    // interested in the password thas was used during the sign-in.
    //
    // Note: The password is always empty on mobile as the user signs in to
    // Chrome with accounts that were added to the device, so Chrome does not
    // have access to the password.
    // DEPRECATED: password will be empty if login is using DICE workflow; the
    // method will be removed once all login is using the DICE workflow.
    virtual void GoogleSigninSucceededWithPassword(
        const AccountInfo& account_info,
        const std::string& password) {}

    // DEPRECATED: Use the above method instead.
    virtual void GoogleSigninSucceeded(const std::string& account_id,
                                       const std::string& username) {}

    // Called when the currently signed-in user for a user has been signed out.
    virtual void GoogleSignedOut(const AccountInfo& account_info) {}

    // DEPRECATED: Use the above method instead.
    virtual void GoogleSignedOut(const std::string& account_id,
                                 const std::string& username) {}

   protected:
    virtual ~Observer() {}

   private:
    // SigninManagers that fire |GoogleSigninSucceededWithPassword|
    // notifications.
    friend class SigninManager;
    friend class FakeSigninManager;
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
                    AccountTrackerService* account_tracker_service,
                    SigninErrorController* signin_error_controller);
#if !defined(OS_CHROMEOS)
 public:
#endif

  ~SigninManagerBase() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers per-install prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // If user was signed in, load tokens from DB if available.
  virtual void Initialize(PrefService* local_state);
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

  // Returns true if there's a signin in progress.
  virtual bool AuthInProgress() const;

  // KeyedService implementation.
  void Shutdown() override;

  // Methods to register or remove observers of signin.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Methods to register or remove SigninDiagnosticObservers.
  void AddSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);
  void RemoveSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);

  // Gives access to the SigninClient instance associated with this instance.
  SigninClient* signin_client() const { return client_; }

  // Adds a callback that will be called when this instance is shut down.Not
  // intended for general usage, but rather for usage only by the Identity
  // Service implementation during the time period of conversion of Chrome to
  // use the Identity Service.
  std::unique_ptr<base::CallbackList<void()>::Subscription>
  RegisterOnShutdownCallback(const base::Closure& cb) {
    return on_shutdown_callback_list_.Add(cb);
  }

 protected:
  AccountTrackerService* account_tracker_service() const {
    return account_tracker_service_;
  }

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

  // List of observers to notify on signin events.
  // Makes sure list is empty on destruction.
  base::ObserverList<Observer, true>::Unchecked observer_list_;

  // Helper method to notify all registered diagnostics observers with.
  void NotifyDiagnosticsObservers(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value);

 private:
  friend class FakeSigninManagerBase;
  friend class FakeSigninManager;

  // Added only to allow SigninManager to call the SigninManagerBase
  // constructor while disallowing any ad-hoc subclassing of
  // SigninManagerBase.
  friend class SigninManager;

  SigninClient* client_;
  AccountTrackerService* account_tracker_service_;
  SigninErrorController* signin_error_controller_;
  bool initialized_;

  // Account id after successful authentication.
  std::string authenticated_account_id_;

  // The list of SigninDiagnosticObservers.
  base::ObserverList<signin_internals_util::SigninDiagnosticsObserver,
                     true>::Unchecked signin_diagnostics_observers_;

  // The list of callbacks notified on shutdown.
  base::CallbackList<void()> on_shutdown_callback_list_;

  base::WeakPtrFactory<SigninManagerBase> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerBase);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_BASE_H_
