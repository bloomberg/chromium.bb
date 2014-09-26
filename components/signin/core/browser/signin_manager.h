// Copyright 2014 The Chromium Authors. All rights reserved.
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

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_

#if defined(OS_CHROMEOS)
// On Chrome OS, SigninManagerBase is all that exists.
#include "components/signin/core/browser/signin_manager_base.h"

#else

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_member.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/merge_session_helper.h"
#include "net/cookies/canonical_cookie.h"

class PrefService;
class ProfileOAuth2TokenService;
class SigninAccountIdHelper;
class SigninClient;

class SigninManager : public SigninManagerBase {
 public:
  // The callback invoked once the OAuth token has been fetched during signin,
  // but before the profile transitions to the "signed-in" state. This allows
  // callers to load policy and prompt the user appropriately before completing
  // signin. The callback is passed the just-fetched OAuth login refresh token.
  typedef base::Callback<void(const std::string&)> OAuthTokenFetchedCallback;

  // Returns true if |url| is a web signin URL and should be hosted in an
  // isolated, privileged signin process.
  static bool IsWebBasedSigninFlowURL(const GURL& url);

  // This is used to distinguish URLs belonging to the special web signin flow
  // running in the special signin process from other URLs on the same domain.
  // We do not grant WebUI privilieges / bindings to this process or to URLs of
  // this scheme; enforcement of privileges is handled separately by
  // OneClickSigninHelper.
  static const char kChromeSigninEffectiveSite[];

  SigninManager(SigninClient* client, ProfileOAuth2TokenService* token_service);
  virtual ~SigninManager();

  // Returns true if the username is allowed based on the policy string.
  static bool IsUsernameAllowedByPolicy(const std::string& username,
                                        const std::string& policy);

  // Attempt to sign in this user with a refresh token.
  // If non-null, the passed |oauth_fetched_callback| callback is invoked once
  // signin has been completed.
  // The callback should invoke SignOut() or CompletePendingSignin() to either
  // continue or cancel the in-process signin.
  virtual void StartSignInWithRefreshToken(
      const std::string& refresh_token,
      const std::string& username,
      const std::string& password,
      const OAuthTokenFetchedCallback& oauth_fetched_callback);

  // Copies auth credentials from one SigninManager to this one. This is used
  // when creating a new profile during the signin process to transfer the
  // in-progress credentials to the new profile.
  virtual void CopyCredentialsFrom(const SigninManager& source);

  // Sign a user out, removing the preference, erasing all keys
  // associated with the user, and canceling all auth in progress.
  virtual void SignOut(signin_metrics::ProfileSignout signout_source_metric);

  // On platforms where SigninManager is responsible for dealing with
  // invalid username policy updates, we need to check this during
  // initialization and sign the user out.
  virtual void Initialize(PrefService* local_state) OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  // Invoked from an OAuthTokenFetchedCallback to complete user signin.
  virtual void CompletePendingSignin();

  // Invoked from SigninManagerAndroid to indicate that the sign-in process
  // has completed for |username|.
  void OnExternalSigninCompleted(const std::string& username);

  // Returns true if there's a signin in progress.
  virtual bool AuthInProgress() const OVERRIDE;

  virtual bool IsSigninAllowed() const OVERRIDE;

  // Returns true if the passed username is allowed by policy. Virtual for
  // mocking in tests.
  virtual bool IsAllowedUsername(const std::string& username) const;

  // If an authentication is in progress, return the username being
  // authenticated. Returns an empty string if no auth is in progress.
  const std::string& GetUsernameForAuthInProgress() const;

  // Set the preference to turn off one-click sign-in so that it won't ever
  // show it again for the user associated with |prefs| (even if the user tries
  // a new account).
  static void DisableOneClickSignIn(PrefService* prefs);

  // Tells the SigninManager whether to prohibit signout for this profile.
  // If |prohibit_signout| is true, then signout will be prohibited.
  void ProhibitSignout(bool prohibit_signout);

  // If true, signout is prohibited for this profile (calls to SignOut() are
  // ignored).
  bool IsSignoutProhibited() const;

  // Add or remove observers for the merge session notification.
  void AddMergeSessionObserver(MergeSessionHelper::Observer* observer);
  void RemoveMergeSessionObserver(MergeSessionHelper::Observer* observer);

 protected:
  // Flag saying whether signing out is allowed.
  bool prohibit_signout_;

 private:
  enum SigninType { SIGNIN_TYPE_NONE, SIGNIN_TYPE_WITH_REFRESH_TOKEN };

  std::string SigninTypeToString(SigninType type);
  friend class FakeSigninManager;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ClearTransientSigninData);
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ProvideSecondFactorSuccess);
  FRIEND_TEST_ALL_PREFIXES(SigninManagerTest, ProvideSecondFactorFailure);

  // If user was signed in, load tokens from DB if available.
  void InitTokenService();

  // Called to setup the transient signin data during one of the
  // StartSigninXXX methods.  |type| indicates which of the methods is being
  // used to perform the signin while |username| and |password| identify the
  // account to be signed in. Returns false and generates an auth error if the
  // passed |username| is not allowed by policy.
  bool PrepareForSignin(SigninType type,
                        const std::string& username,
                        const std::string& password);

  // Persists |username| as the currently signed-in account, and triggers
  // a sign-in success notification.
  void OnSignedIn(const std::string& username);

  // Called when a new request to re-authenticate a user is in progress.
  // Will clear in memory data but leaves the db as such so when the browser
  // restarts we can use the old token(which might throw a password error).
  void ClearTransientSigninData();

  // Called to handle an error from a GAIA auth fetch.  Sets the last error
  // to |error|, sends out a notification of login failure and clears the
  // transient signin data.
  void HandleAuthError(const GoogleServiceAuthError& error);

  void OnSigninAllowedPrefChanged();
  void OnGoogleServicesUsernamePatternChanged();

  // ClientLogin identity.
  std::string possibly_invalid_username_;
  std::string password_;  // This is kept empty whenever possible.

  // Fetcher for the obfuscated user id.
  scoped_ptr<SigninAccountIdHelper> account_id_helper_;

  // The type of sign being performed.  This value is valid only between a call
  // to one of the StartSigninXXX methods and when the sign in is either
  // successful or not.
  SigninType type_;

  // Temporarily saves the oauth2 refresh token.  It will be passed to the
  // token service so that it does not need to mint new ones.
  std::string temp_refresh_token_;

  // The SigninClient object associated with this object. Must outlive this
  // object.
  SigninClient* client_;

  // The ProfileOAuth2TokenService instance associated with this object. Must
  // outlive this object.
  ProfileOAuth2TokenService* token_service_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  // Helper to merge signed in account into the content area.
  scoped_ptr<MergeSessionHelper> merge_session_helper_;

  base::WeakPtrFactory<SigninManager> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManager);
};

#endif  // !defined(OS_CHROMEOS)

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_MANAGER_H_
