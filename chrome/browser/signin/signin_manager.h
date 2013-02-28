// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking
// which user is signed in. When a user is signed in, a ClientLogin
// request is run on their behalf. Auth tokens are fetched from Google
// and the results are stored in the TokenService.
//
// **NOTE** on semantics of SigninManager:
//
// Once a signin is successful, the username becomes "established" and will not
// be cleared until a SignOut operation is performed (persists across
// restarts). Until that happens, the signin manager can still be used to
// refresh credentials, but changing the username is not permitted.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/ubertoken_fetcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/canonical_cookie.h"

class CookieSettings;
class GaiaAuthFetcher;
class ProfileIOData;
class PrefService;
class SigninGlobalError;

namespace policy {
class CloudPolicyClient;
}

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

class SigninManager : public GaiaAuthConsumer,
                      public UbertokenConsumer,
                      public content::NotificationObserver,
                      public ProfileKeyedService {
 public:
  // Methods to register or remove SigninDiagnosticObservers
  void AddSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);
  void RemoveSigninDiagnosticsObserver(
      signin_internals_util::SigninDiagnosticsObserver* observer);

  // Returns true if the cookie policy for the given profile allows cookies
  // for the Google signin domain.
  static bool AreSigninCookiesAllowed(Profile* profile);
  static bool AreSigninCookiesAllowed(CookieSettings* cookie_settings);

  // Returns true if the username is allowed based on the policy string.
  static bool IsAllowedUsername(const std::string& username,
                                const std::string& policy);

  SigninManager();
  virtual ~SigninManager();

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

  // Attempt to sign in this user with ClientLogin. If successful, set a
  // preference indicating the signed in user and send out a notification,
  // then start fetching tokens for the user.
  // This is overridden for test subclasses that don't want to issue auth
  // requests.
  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& login_token,
                           const std::string& login_captcha);

  // Used when a second factor access code was required to complete a signin
  // attempt.
  void ProvideSecondFactorAccessCode(const std::string& access_code);

  // Attempt to sign in this user with existing credentials from the cookie jar.
  // |session_index| indicates which user account to use if the cookie jar
  // contains a multi-login session. Otherwise the end result of this call is
  // the same as StartSignIn().
  virtual void StartSignInWithCredentials(const std::string& session_index,
                                          const std::string& username,
                                          const std::string& password);

  // Attempt to sign in this user with ClientOAuth. If successful, set a
  // preference indicating the signed in user and send out a notification,
  // then start fetching tokens for the user.
  virtual void StartSignInWithOAuth(const std::string& username,
                                    const std::string& password);

  // Provide a challenge solution to a failed signin attempt with
  // StartSignInWithOAuth().  |type| and |token| come from the
  // GoogleServiceAuthError of the failed attempt.
  // |solution| is the answer typed by the user.
  void ProvideOAuthChallengeResponse(GoogleServiceAuthError::State type,
                                     const std::string& token,
                                     const std::string& solution);

  // Sign a user out, removing the preference, erasing all keys
  // associated with the user, and canceling all auth in progress.
  virtual void SignOut();

  // Returns true if there's a signin in progress. Virtual so it can be
  // overridden by mocks.
  virtual bool AuthInProgress() const;

  // Handles errors if a required user info key is not returned from the
  // GetUserInfo call.
  void OnGetUserInfoKeyNotFound(const std::string& key);

  // Set the profile preference to turn off one-click sign-in so that it won't
  // ever show it again in this profile (even if the user tries a new account).
  static void DisableOneClickSignIn(Profile* profile);

  // GaiaAuthConsumer
  virtual void OnClientLoginSuccess(const ClientLoginResult& result) OVERRIDE;
  virtual void OnClientLoginFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnClientOAuthSuccess(const ClientOAuthResult& result) OVERRIDE;
  virtual void OnClientOAuthFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnGetUserInfoSuccess(const UserInfoMap& data) OVERRIDE;
  virtual void OnGetUserInfoFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // UbertokenConsumer
  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  SigninGlobalError* signin_global_error() {
    return signin_global_error_.get();
  }

  const SigninGlobalError* signin_global_error() const {
    return signin_global_error_.get();
  }

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Tells the SigninManager to prohibit signout for this profile.
  void ProhibitSignout();

  // If true, signout is prohibited for this profile (calls to SignOut() are
  // ignored).
  bool IsSignoutProhibited() const;

 protected:
  // Weak pointer to parent profile (protected so FakeSigninManager can access
  // it).
  Profile* profile_;

  // Used to show auth errors in the wrench menu. The SigninGlobalError is
  // different than most GlobalErrors in that its lifetime is controlled by
  // SigninManager (so we can expose a reference for use in the wrench menu).
  scoped_ptr<SigninGlobalError> signin_global_error_;

  // Flag saying whether signing out is allowed.
  bool prohibit_signout_;

 private:
  enum SigninType {
    SIGNIN_TYPE_NONE,
    SIGNIN_TYPE_CLIENT_LOGIN,
    SIGNIN_TYPE_WITH_CREDENTIALS,
    SIGNIN_TYPE_CLIENT_OAUTH,
  };

  std::string SigninTypeToString(SigninType type);

  friend class FakeSigninManager;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ClearTransientSigninData);
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ProvideSecondFactorSuccess);
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ProvideSecondFactorFailure);

  // Called to setup the transient signin data during one of the
  // StartSigninXXX methods.  |type| indicates which of the methods is being
  // used to perform the signin while |username| and |password| identify the
  // account to be signed in. Returns false and generates an auth error if the
  // passed |username| is not allowed by policy.
  bool PrepareForSignin(SigninType type,
                        const std::string& username,
                        const std::string& password);

  // Called to verify GAIA cookies asynchronously before starting auto sign-in
  // without password.
  void VerifyGaiaCookiesBeforeSignIn(const std::string& session_index);

  // Called when GAIA cookies are fetched. If LSID cookie is valid, then start
  // auto sign-in by exchanging cookies for an oauth code.
  void OnGaiaCookiesFetched(
      const std::string session_index, const net::CookieList& cookie_list);

  // Called when a new request to re-authenticate a user is in progress.
  // Will clear in memory data but leaves the db as such so when the browser
  // restarts we can use the old token(which might throw a password error).
  void ClearTransientSigninData();

  // Called to handle an error from a GAIA auth fetch.  Sets the last error
  // to |error|, sends out a notification of login failure, and clears the
  // transient signin data if |clear_transient_data| is true.
  void HandleAuthError(const GoogleServiceAuthError& error,
                       bool clear_transient_data);

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
  // Callback invoked once policy registration is complete. If registration
  // fails, |client| will be null.
  void OnRegisteredForPolicy(scoped_ptr<policy::CloudPolicyClient> client);

  // Callback invoked when a policy fetch request has completed. |success| is
  // true if policy was successfully fetched.
  void OnPolicyFetchComplete(bool success);

  // Called to create a new profile, which is then signed in with the
  // in-progress auth credentials currently stored in this object.
  void TransferCredentialsToNewProfile();

  // Helper function that loads policy with the passed CloudPolicyClient, then
  // completes the signin process.
  void LoadPolicyWithCachedClient();

  // Callback invoked once a profile is created, so we can complete the
  // credentials transfer and load policy.
  void CompleteSigninForNewProfile(Profile* profile,
                                   Profile::CreateStatus status);
#endif  // defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)

  // Invoked once policy has been loaded to complete user signin.
  void CompleteSigninAfterPolicyLoad();

  // ClientLogin identity.
  std::string possibly_invalid_username_;
  std::string password_;  // This is kept empty whenever possible.
  bool had_two_factor_error_;

  void CleanupNotificationRegistration();

  void OnGoogleServicesUsernamePatternChanged();

  void OnSigninAllowedPrefChanged();

  // Helper methods to notify all registered diagnostics observers with.
  void NotifyDiagnosticsObservers(
      const signin_internals_util::UntimedSigninStatusField& field,
      const std::string& value);
  void NotifyDiagnosticsObservers(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value);

  // Result of the last client login, kept pending the lookup of the
  // canonical email.
  ClientLoginResult last_result_;

  // Actual client login handler.
  scoped_ptr<GaiaAuthFetcher> client_login_;

  // Registrar for notifications from the TokenService.
  content::NotificationRegistrar registrar_;

  // UbertokenFetcher to login to user to the web property.
  scoped_ptr<UbertokenFetcher> ubertoken_fetcher_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  // Actual username after successful authentication.
  std::string authenticated_username_;

  // The type of sign being performed.  This value is valid only between a call
  // to one of the StartSigninXXX methods and when the sign in is either
  // successful or not.
  SigninType type_;

  // Temporarily saves the oauth2 refresh and access tokens when signing in
  // with credentials.  These will be passed to TokenService so that it does
  // not need to mint new ones.
  ClientOAuthResult temp_oauth_login_tokens_;

  // The list of SigninDiagnosticObservers.
  ObserverList<signin_internals_util::SigninDiagnosticsObserver, true>
      signin_diagnostics_observers_;

  base::WeakPtrFactory<SigninManager> weak_pointer_factory_;


#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
  // CloudPolicyClient reference we keep while determining whether to create
  // a new profile for an enterprise user or not.
  scoped_ptr<policy::CloudPolicyClient> policy_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SigninManager);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
