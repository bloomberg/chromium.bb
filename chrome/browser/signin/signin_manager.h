// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The signin manager encapsulates some functionality tracking
// which user is signed in. See SigninManagerBase for full description of
// responsibilities. The class defined in this file provides functionality
// required by all platforms except Chrome OS.
//
// When a user is signed in, a ClientLogin request is run on their behalf.
// Auth tokens are fetched from Google and the results are stored in the
// TokenService.
// TODO(tim): Bug 92948, 226464. ClientLogin is all but gone from use.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_

#if defined(OS_CHROMEOS)
// On Chrome OS, SigninManagerBase is all that exists.
#include "chrome/browser/signin/signin_manager_base.h"

#else

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
#include "chrome/browser/signin/signin_manager_base.h"
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

class SigninManager : public SigninManagerBase,
                      public GaiaAuthConsumer,
                      public UbertokenConsumer,
                      public content::NotificationObserver {
 public:
  // Returns true if |url| is a web signin URL and should be hosted in an
  // isolated, privileged signin process.
  static bool IsWebBasedSigninFlowURL(const GURL& url);

  // This is used to distinguish URLs belonging to the special web signin flow
  // running in the special signin process from other URLs on the same domain.
  // We do not grant WebUI privilieges / bindings to this process or to URLs of
  // this scheme; enforcement of privileges is handled separately by
  // OneClickSigninHelper.
  static const char* kChromeSigninEffectiveSite;

  SigninManager();
  virtual ~SigninManager();

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
  virtual void SignOut() OVERRIDE;

  // Returns true if there's a signin in progress.
  virtual bool AuthInProgress() const OVERRIDE;

  // If an authentication is in progress, return the username being
  // authenticated. Returns an empty string if no auth is in progress.
  const std::string& GetUsernameForAuthInProgress() const;

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
  virtual void OnOAuth2RevokeTokenCompleted() OVERRIDE;
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


  // Tells the SigninManager whether to prohibit signout for this profile.
  // If |prohibit_signout| is true, then signout will be prohibited.
  void ProhibitSignout(bool prohibit_signout);

  // If true, signout is prohibited for this profile (calls to SignOut() are
  // ignored).
  bool IsSignoutProhibited() const;

  // Allows the SigninManager to track the privileged signin process
  // identified by |process_id| so that we can later ask (via IsSigninProcess)
  // if it is safe to sign the user in from the current context (see
  // OneClickSigninHelper).  All of this tracking state is reset once the
  // renderer process terminates.
  void SetSigninProcess(int process_id);
  bool IsSigninProcess(int process_id) const;
  bool HasSigninProcess() const;

 protected:
  // If user was signed in, load tokens from DB if available.
  virtual void InitTokenService() OVERRIDE;

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

  // Called to tell GAIA that we will no longer be using the current refresh
  // token.
  void RevokeOAuthLoginToken();

#if defined(ENABLE_CONFIGURATION_POLICY)
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

  // Cancels the in-progress signin for this profile.
  void CancelSignin();

#endif  // defined(ENABLE_CONFIGURATION_POLICY)

  // Invoked once policy has been loaded to complete user signin.
  void CompleteSigninAfterPolicyLoad();

  // ClientLogin identity.
  std::string possibly_invalid_username_;
  std::string password_;  // This is kept empty whenever possible.
  bool had_two_factor_error_;

  void CleanupNotificationRegistration();

  // Result of the last client login, kept pending the lookup of the
  // canonical email.
  ClientLoginResult last_result_;

  // Actual client login handler.
  scoped_ptr<GaiaAuthFetcher> client_login_;

  // Registrar for notifications from the TokenService.
  content::NotificationRegistrar registrar_;

  // UbertokenFetcher to login to user to the web property.
  scoped_ptr<UbertokenFetcher> ubertoken_fetcher_;

  // OAuth revocation fetcher for sign outs.
  scoped_ptr<GaiaAuthFetcher> revoke_token_fetcher_;

  // The type of sign being performed.  This value is valid only between a call
  // to one of the StartSigninXXX methods and when the sign in is either
  // successful or not.
  SigninType type_;

  // Temporarily saves the oauth2 refresh and access tokens when signing in
  // with credentials.  These will be passed to TokenService so that it does
  // not need to mint new ones.
  ClientOAuthResult temp_oauth_login_tokens_;

  base::WeakPtrFactory<SigninManager> weak_pointer_factory_;

  // See SetSigninProcess.  Tracks the currently active signin process
  // by ID, if there is one.
  int signin_process_id_;

#if defined(ENABLE_CONFIGURATION_POLICY)
  // CloudPolicyClient reference we keep while determining whether to create
  // a new profile for an enterprise user or not.
  scoped_ptr<policy::CloudPolicyClient> policy_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SigninManager);
};

#endif  // !defined(OS_CHROMEOS)

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_H_
