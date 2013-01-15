// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_LOGIN_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_LOGIN_MANAGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/oauth1_login_verifier.h"
#include "chrome/browser/chromeos/login/oauth1_token_fetcher.h"
#include "chrome/browser/chromeos/login/oauth_login_manager.h"
#include "chrome/browser/chromeos/login/policy_oauth_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

class Profile;

namespace chromeos {

// OAuth1 specialization of OAuthLoginManager.
// TODO(zelidrag): Get rid of this one once we move everything to OAuth2.
class OAuth1LoginManager : public OAuthLoginManager,
                           public OAuth1TokenFetcher::Delegate,
                           public OAuth1LoginVerifier::Delegate {
 public:
  explicit OAuth1LoginManager(OAuthLoginManager::Delegate* delegate);
  virtual ~OAuth1LoginManager();
  // OAuthLoginManager overrides.
  virtual void RestorePolicyTokens(
      net::URLRequestContextGetter* auth_request_context) OVERRIDE;
  virtual void RestoreSession(
      Profile* user_profile,
      net::URLRequestContextGetter* auth_request_context,
      bool restore_from_auth_cookies) OVERRIDE;
  virtual void ContinueSessionRestore() OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // OAuth1TokenFetcher::Delegate overrides.
  virtual void OnOAuth1AccessTokenAvailable(const std::string& token,
                                            const std::string& secret) OVERRIDE;
  virtual void OnOAuth1AccessTokenFetchFailed() OVERRIDE;

  // OAuth1LoginVerifier::Delegate overrides.
  virtual void OnOAuth1VerificationSucceeded(const std::string& user_name,
                                             const std::string& sid,
                                             const std::string& lsid,
                                             const std::string& auth) OVERRIDE;
  virtual void OnOAuth1VerificationFailed(
      const std::string& user_name) OVERRIDE;

  // Reads OAuth1 token from user profile's prefs.
  bool ReadOAuth1Tokens();

  // Stores OAuth1 token + secret in profile's prefs.
  void StoreOAuth1Tokens();

  // Fetch user credentials (sid/lsid) from |oauth1_token_| and
  // |oauth1_secret_|.
  void FetchCredentialsWithOAuth1();

  // Verifies OAuth1 token by performing OAuthLogin and fetching credentials.
  void VerifyOAuth1AccessToken();

  // Starts fetching device policy tokens.
  void FetchPolicyTokens();

  scoped_ptr<OAuth1TokenFetcher> oauth1_token_fetcher_;
  scoped_ptr<OAuth1LoginVerifier> oauth1_login_verifier_;
  scoped_ptr<PolicyOAuthFetcher> policy_oauth_fetcher_;
  std::string oauth1_token_;
  std::string oauth1_secret_;

  DISALLOW_COPY_AND_ASSIGN(OAuth1LoginManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH1_LOGIN_MANAGER_H_
