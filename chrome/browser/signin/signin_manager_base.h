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
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/signin/signin_internals_util.h"

class CookieSettings;
class ProfileIOData;
class PrefService;
class SigninGlobalError;

// Details for the Notification type GOOGLE_SIGNIN_SUCCESSFUL.
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
struct GoogleServiceSignoutDetails {
  explicit GoogleServiceSignoutDetails(const std::string& in_username)
      : username(in_username) {}
  std::string username;
};

class SigninManagerBase : public ProfileKeyedService {
 public:
  // Returns true if the username is allowed based on the policy string.
  static bool IsAllowedUsername(const std::string& username,
                                const std::string& policy);

  SigninManagerBase();
  virtual ~SigninManagerBase();

  // If user was signed in, load tokens from DB if available.
  void Initialize(Profile* profile);
  bool IsInitialized() const;

  // Returns true if the passed username is allowed by policy. Virtual for
  // mocking in tests.
  virtual bool IsAllowedUsername(const std::string& username) const;

  // Returns true if a signin to Chrome is allowed (by policy or pref).
  bool IsSigninAllowed() const;

  // Checks if signin is allowed for the profile that owns |io_data|. This must
  // be invoked on the IO thread, and can be used to check if signin is enabled
  // on that thread.
  static bool IsSigninAllowedOnIOThread(ProfileIOData* io_data);

  // If a user has previously established a username and SignOut has not been
  // called, this will return the username.
  // Otherwise, it will return an empty string.
  const std::string& GetAuthenticatedUsername() const;

  // Sets the user name.  Note: |username| should be already authenticated as
  // this is a sticky operation (in contrast to StartSignIn).
  // TODO(tim): Remove this in favor of passing username on construction by
  // (by platform / depending on StartBehavior). Bug 88109.
  void SetAuthenticatedUsername(const std::string& username);

  // Sign a user out, removing the preference, erasing all keys
  // associated with the user, and canceling all auth in progress.
  // TODO(tim): Remove SignOut here, it belongs in the derived class.
  // Bug 174927.
  virtual void SignOut();

  // Returns true if there's a signin in progress.
  virtual bool AuthInProgress() const;

  SigninGlobalError* signin_global_error() {
    return signin_global_error_.get();
  }

  const SigninGlobalError* signin_global_error() const {
    return signin_global_error_.get();
  }

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

    // Methods to register or remove SigninDiagnosticObservers
  void AddSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);
  void RemoveSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);

 protected:
  // Lets different platforms initialize TokenService in their own way.
  virtual void InitTokenService();

  // Pointer to parent profile (protected so FakeSigninManager can access
  // it).
  Profile* profile_;

  // Used to show auth errors in the wrench menu. The SigninGlobalError is
  // different than most GlobalErrors in that its lifetime is controlled by
  // SigninManager (so we can expose a reference for use in the wrench menu).
  scoped_ptr<SigninGlobalError> signin_global_error_;

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
  void OnGoogleServicesUsernamePatternChanged();

  void OnSigninAllowedPrefChanged();

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  // Actual username after successful authentication.
  std::string authenticated_username_;

  // The list of SigninDiagnosticObservers.
  ObserverList<signin_internals_util::SigninDiagnosticsObserver, true>
      signin_diagnostics_observers_;

  base::WeakPtrFactory<SigninManagerBase> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerBase);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_BASE_H_
