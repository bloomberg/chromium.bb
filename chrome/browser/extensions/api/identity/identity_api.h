// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/browser/extensions/api/identity/identity_signin_flow.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

class GetAuthTokenFunctionTest;
class MockGetAuthTokenFunction;
class GoogleServiceAuthError;
class Profile;
class SigninManagerBase;

namespace extensions {

namespace identity_constants {
extern const char kInvalidClientId[];
extern const char kInvalidScopes[];
extern const char kAuthFailure[];
extern const char kNoGrant[];
extern const char kUserRejected[];
extern const char kUserNotSignedIn[];
extern const char kInteractionRequired[];
extern const char kInvalidRedirect[];
extern const char kOffTheRecord[];
}  // namespace identity_constants

// identity.getAuthToken fetches an OAuth 2 function for the
// caller. The request has three sub-flows: non-interactive,
// interactive, and sign-in.
//
// In the non-interactive flow, getAuthToken requests a token from
// GAIA. GAIA may respond with a token, an error, or "consent
// required". In the consent required cases, getAuthToken proceeds to
// the second, interactive phase.
//
// The interactive flow presents a scope approval dialog to the
// user. If the user approves the request, a grant will be recorded on
// the server, and an access token will be returned to the caller.
//
// In some cases we need to display a sign-in dialog. Normally the
// profile will be signed in already, but if it turns out we need a
// new login token, there is a sign-in flow. If that flow completes
// successfully, getAuthToken proceeds to the non-interactive flow.
class IdentityGetAuthTokenFunction : public AsyncExtensionFunction,
                                     public ExtensionInstallPrompt::Delegate,
                                     public IdentityMintRequestQueue::Request,
                                     public OAuth2MintTokenFlow::Delegate,
                                     public IdentitySigninFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN);

  IdentityGetAuthTokenFunction();

 protected:
  virtual ~IdentityGetAuthTokenFunction();

 private:
  friend class GetAuthTokenFunctionTest;
  friend class MockGetAuthTokenFunction;

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Helpers to report async function results to the caller.
  void CompleteFunctionWithResult(const std::string& access_token);
  void CompleteFunctionWithError(const std::string& error);

  // Initiate/complete the sub-flows.
  void StartSigninFlow();
  void StartMintTokenFlow(IdentityMintRequestQueue::MintType type);
  void CompleteMintTokenFlow();

  // IdentityMintRequestQueue::Request implementation:
  virtual void StartMintToken(IdentityMintRequestQueue::MintType type) OVERRIDE;

  // OAuth2MintTokenFlow::Delegate implementation:
  virtual void OnMintTokenSuccess(const std::string& access_token,
                                  int time_to_live) OVERRIDE;
  virtual void OnMintTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnIssueAdviceSuccess(
      const IssueAdviceInfo& issue_advice) OVERRIDE;

  // IdentitySigninFlow::Delegate implementation:
  virtual void SigninSuccess(const std::string& token) OVERRIDE;
  virtual void SigninFailed() OVERRIDE;

  // ExtensionInstallPrompt::Delegate implementation:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Starts a mint token request to GAIA.
  void StartGaiaRequest(OAuth2MintTokenFlow::Mode mode);

  // Methods for invoking UI. Overridable for testing.
  virtual void ShowLoginPopup();
  virtual void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice);
  // Caller owns the returned instance.
  virtual OAuth2MintTokenFlow* CreateMintTokenFlow(
      OAuth2MintTokenFlow::Mode mode);

  // Checks if there is a master login token to mint tokens for the extension.
  virtual bool HasLoginToken() const;

  bool should_prompt_for_scopes_;
  IdentityMintRequestQueue::MintType mint_token_flow_type_;
  scoped_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  std::string refresh_token_;
  bool should_prompt_for_signin_;

  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  IssueAdviceInfo issue_advice_;
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_ptr<IdentitySigninFlow> signin_flow_;
};

class IdentityRemoveCachedAuthTokenFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.removeCachedAuthToken",
                             EXPERIMENTAL_IDENTITY_REMOVECACHEDAUTHTOKEN)
  IdentityRemoveCachedAuthTokenFunction();

 protected:
  virtual ~IdentityRemoveCachedAuthTokenFunction();

  // SyncExtensionFunction implementation:
  virtual bool RunImpl() OVERRIDE;
};

class IdentityLaunchWebAuthFlowFunction : public AsyncExtensionFunction,
                                          public WebAuthFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.launchWebAuthFlow",
                             EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW);

  IdentityLaunchWebAuthFlowFunction();

  // URL checking helpers. Public for testing.
  // Checks to see if the current URL ends the flow.
  bool IsFinalRedirectURL(const GURL& url) const;

  // Unit tests may override extension_id.
  void InitFinalRedirectURLPrefixesForTest(const std::string& extension_id);

 private:
  virtual ~IdentityLaunchWebAuthFlowFunction();
  virtual bool RunImpl() OVERRIDE;

  // WebAuthFlow::Delegate implementation.
  virtual void OnAuthFlowFailure(WebAuthFlow::Failure failure) OVERRIDE;
  virtual void OnAuthFlowURLChange(const GURL& redirect_url) OVERRIDE;

  // Helper to initialize final URLs vector.
  void InitFinalRedirectURLPrefixes(const std::string& extension_id);

  scoped_ptr<WebAuthFlow> auth_flow_;
  std::vector<GURL> final_prefixes_;
};

class IdentityTokenCacheValue {
 public:
  IdentityTokenCacheValue();
  explicit IdentityTokenCacheValue(const IssueAdviceInfo& issue_advice);
  IdentityTokenCacheValue(const std::string& token,
                          base::TimeDelta time_to_live);
  ~IdentityTokenCacheValue();

  // Order of these entries is used to determine whether or not new
  // entries supercede older ones in SetCachedToken.
  enum CacheValueStatus {
    CACHE_STATUS_NOTFOUND,
    CACHE_STATUS_ADVICE,
    CACHE_STATUS_TOKEN
  };

  CacheValueStatus status() const;
  const IssueAdviceInfo& issue_advice() const;
  const std::string& token() const;

 private:
  bool is_expired() const;

  CacheValueStatus status_;
  IssueAdviceInfo issue_advice_;
  std::string token_;
  base::Time expiration_time_;
};

class IdentityAPI : public ProfileKeyedAPI,
                    public SigninGlobalError::AuthStatusProvider,
                    public content::NotificationObserver {
 public:
  explicit IdentityAPI(Profile* profile);
  virtual ~IdentityAPI();
  void Initialize();

  // Request serialization queue for getAuthToken.
  IdentityMintRequestQueue* mint_queue();

  // Token cache
  void SetCachedToken(const std::string& extension_id,
                      const std::vector<std::string> scopes,
                      const IdentityTokenCacheValue& token_data);
  void EraseCachedToken(const std::string& extension_id,
                        const std::string& token);
  void EraseAllCachedTokens();
  const IdentityTokenCacheValue& GetCachedToken(
      const std::string& extension_id, const std::vector<std::string> scopes);

  void ReportAuthError(const GoogleServiceAuthError& error);

  // ProfileKeyedAPI implementation.
  virtual void Shutdown() OVERRIDE;
  static ProfileKeyedAPIFactory<IdentityAPI>* GetFactoryInstance();

  // AuthStatusProvider implementation.
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<IdentityAPI>;

  struct TokenCacheKey {
    TokenCacheKey(const std::string& extension_id,
                  const std::set<std::string> scopes);
    ~TokenCacheKey();
    bool operator<(const TokenCacheKey& rhs) const;
    std::string extension_id;
    std::set<std::string> scopes;
  };

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "IdentityAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  Profile* profile_;
  SigninManagerBase* signin_manager_;
  GoogleServiceAuthError error_;
  // Used to listen to notifications from the TokenService.
  content::NotificationRegistrar registrar_;
  IdentityMintRequestQueue mint_queue_;
  std::map<TokenCacheKey, IdentityTokenCacheValue> token_cache_;
};

template <>
void ProfileKeyedAPIFactory<IdentityAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
