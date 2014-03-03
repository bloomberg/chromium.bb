// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_BASE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_BASE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

class CookieSettings;
class ProfileIOData;
class PrefService;

// Details for the Notification type GOOGLE_SIGNIN_SUCCESSFUL.
// TODO(blundell): Eliminate this struct once crbug.com/333997 is fixed.
// A listener might use this to make note of a username / password
// pair for encryption keys.
struct GoogleServiceSigninSuccessDetails {
  GoogleServiceSigninSuccessDetails(const std::string& in_username,
                                    const std::string& in_password)
      : username(in_username),
        password(in_password) {}
  std::string username;
  std::string password;
};

// Details for the Notification type NOTIFICATION_GOOGLE_SIGNED_OUT.
// TODO(blundell): Eliminate this struct once crbug.com/333997 is fixed.
struct GoogleServiceSignoutDetails {
  explicit GoogleServiceSignoutDetails(const std::string& in_username)
      : username(in_username) {}
  std::string username;
};

class SigninManagerBase : public BrowserContextKeyedService {
 public:
  class Observer {
   public:
    // Called when a user fails to sign into Google services such as sync.
    virtual void GoogleSigninFailed(const GoogleServiceAuthError& error) {}

    // Called when a user signs into Google services such as sync.
    virtual void GoogleSigninSucceeded(const std::string& username,
                                       const std::string& password) {}

    // Called when the currently signed-in user for a user has been signed out.
    virtual void GoogleSignedOut(const std::string& username) {}

   protected:
    virtual ~Observer() {}
  };

  SigninManagerBase();
  virtual ~SigninManagerBase();

  // If user was signed in, load tokens from DB if available.
  virtual void Initialize(Profile* profile, PrefService* local_state);
  bool IsInitialized() const;

  // Returns true if a signin to Chrome is allowed (by policy or pref).
  // TODO(tim): kSigninAllowed is defined for all platforms in pref_names.h.
  // If kSigninAllowed pref was non-Chrome OS-only, this method wouldn't be
  // needed, but as is we provide this method to let all interested code
  // code query the value in one way, versus half using PrefService directly
  // and the other half using SigninManager.
  virtual bool IsSigninAllowed() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the normalized email address of the account. Otherwise, it returns an empty
  // string.
  const std::string& GetAuthenticatedUsername() const;

  // If a user has previously signed in (and has not signed out), this returns
  // the account id. Otherwise, it returns an empty string.  This id can be used
  // to uniquely identify an account, so for example can be used as a key to
  // map accounts to data.
  //
  // TODO(rogerta): eventually the account id should be an obfuscated gaia id.
  // For now though, this function returns the same value as
  // GetAuthenticatedUsername() since lots of code assumes the unique id for an
  // account is the username.  For code that needs a unique id to represent the
  // connected account, call this method. Example: the AccountInfoMap type
  // in MutableProfileOAuth2TokenService.  For code that needs to know the
  // normalized email address of the connected account, use
  // GetAuthenticatedUsername().  Example: to show the string "Signed in as XXX"
  // in the hotdog menu.
  const std::string& GetAuthenticatedAccountId() const;

  // Sets the user name.  Note: |username| should be already authenticated as
  // this is a sticky operation (in contrast to StartSignIn).
  // TODO(tim): Remove this in favor of passing username on construction by
  // (by platform / depending on StartBehavior). Bug 88109.
  void SetAuthenticatedUsername(const std::string& username);

  // Returns true if there's a signin in progress.
  virtual bool AuthInProgress() const;

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Methods to register or remove observers of signin.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Methods to register or remove SigninDiagnosticObservers.
  void AddSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);
  void RemoveSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);

  Profile* profile() { return profile_; }

 protected:
  // Used by subclass to clear authenticated_username_ instead of using
  // SetAuthenticatedUsername, which enforces special preconditions due
  // to the fact that it is part of the public API and called by clients.
  void clear_authenticated_username();

  // Pointer to parent profile (protected so FakeSigninManager can access
  // it).
  Profile* profile_;

  // List of observers to notify on signin events.
  // Makes sure list is empty on destruction.
  ObserverList<Observer, true> observer_list_;

  // Helper methods to notify all registered diagnostics observers with.
  void NotifyDiagnosticsObservers(
      const signin_internals_util::UntimedSigninStatusField& field,
      const std::string& value);
  void NotifyDiagnosticsObservers(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value);

 private:
  friend class FakeSigninManagerBase;
  friend class FakeSigninManager;

  // Actual username after successful authentication.
  std::string authenticated_username_;

  // The list of SigninDiagnosticObservers.
  ObserverList<signin_internals_util::SigninDiagnosticsObserver, true>
      signin_diagnostics_observers_;

  base::WeakPtrFactory<SigninManagerBase> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerBase);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_BASE_H_
