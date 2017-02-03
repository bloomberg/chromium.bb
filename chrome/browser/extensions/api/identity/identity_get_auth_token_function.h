// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_

#include "chrome/browser/extensions/api/identity/gaia_web_auth_flow.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/browser/extensions/api/identity/identity_signin_flow.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace extensions {

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
class IdentityGetAuthTokenFunction : public ChromeAsyncExtensionFunction,
                                     public GaiaWebAuthFlow::Delegate,
                                     public IdentityMintRequestQueue::Request,
                                     public OAuth2MintTokenFlow::Delegate,
                                     public IdentitySigninFlow::Delegate,
                                     public OAuth2TokenService::Consumer {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN);

  IdentityGetAuthTokenFunction();

  const ExtensionTokenKey* GetExtensionTokenKeyForTest() {
    return token_key_.get();
  }

  void Shutdown();

 protected:
  ~IdentityGetAuthTokenFunction() override;

  // IdentitySigninFlow::Delegate implementation:
  void SigninSuccess() override;
  void SigninFailed() override;

  // GaiaWebAuthFlow::Delegate implementation:
  void OnGaiaFlowFailure(GaiaWebAuthFlow::Failure failure,
                         GoogleServiceAuthError service_error,
                         const std::string& oauth_error) override;
  void OnGaiaFlowCompleted(const std::string& access_token,
                           const std::string& expiration) override;

  // Starts a login access token request.
  virtual void StartLoginAccessTokenRequest();

  // OAuth2TokenService::Consumer implementation:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Starts a mint token request to GAIA.
  // Exposed for testing.
  virtual void StartGaiaRequest(const std::string& login_access_token);

  // Caller owns the returned instance.
  // Exposed for testing.
  virtual OAuth2MintTokenFlow* CreateMintTokenFlow();

  std::unique_ptr<OAuth2TokenService::Request> login_token_request_;

 private:
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithChromeClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithNormalClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, InteractiveQueueShutdown);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, NoninteractiveShutdown);

  // ExtensionFunction:
  bool RunAsync() override;

  // Helpers to report async function results to the caller.
  void StartAsyncRun();
  void CompleteAsyncRun(bool success);
  void CompleteFunctionWithResult(const std::string& access_token);
  void CompleteFunctionWithError(const std::string& error);

  // Initiate/complete the sub-flows.
  void StartSigninFlow();
  void StartMintTokenFlow(IdentityMintRequestQueue::MintType type);
  void CompleteMintTokenFlow();

  // IdentityMintRequestQueue::Request implementation:
  void StartMintToken(IdentityMintRequestQueue::MintType type) override;

  // OAuth2MintTokenFlow::Delegate implementation:
  void OnMintTokenSuccess(const std::string& access_token,
                          int time_to_live) override;
  void OnMintTokenFailure(const GoogleServiceAuthError& error) override;
  void OnIssueAdviceSuccess(const IssueAdviceInfo& issue_advice) override;

#if defined(OS_CHROMEOS)
  // Starts a login access token request for device robot account. This method
  // will be called only in Chrome OS for:
  // 1. Enterprise kiosk mode.
  // 2. Whitelisted first party apps in public session.
  virtual void StartDeviceLoginAccessTokenRequest();

  bool IsOriginWhitelistedInPublicSession();
#endif

  // Methods for invoking UI. Overridable for testing.
  virtual void ShowLoginPopup();
  virtual void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice);

  // Checks if there is a master login token to mint tokens for the extension.
  bool HasLoginToken() const;

  // Maps OAuth2 protocol errors to an error message returned to the
  // developer in chrome.runtime.lastError.
  std::string MapOAuth2ErrorToDescription(const std::string& error);

  std::string GetOAuth2ClientId() const;

  bool interactive_;
  bool should_prompt_for_scopes_;
  IdentityMintRequestQueue::MintType mint_token_flow_type_;
  std::unique_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  OAuth2MintTokenFlow::Mode gaia_mint_token_mode_;
  bool should_prompt_for_signin_;

  std::unique_ptr<ExtensionTokenKey> token_key_;
  std::string oauth2_client_id_;
  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  IssueAdviceInfo issue_advice_;
  std::unique_ptr<GaiaWebAuthFlow> gaia_web_auth_flow_;
  std::unique_ptr<IdentitySigninFlow> signin_flow_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_
