// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXPERIMENTAL_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXPERIMENTAL_IDENTITY_API_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/browser/extensions/api/identity/identity_signin_flow.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

namespace extensions {

// TODO(courage): These functions exist to support some apps that were
// whitelisted to use the experimental API. Remove them once those
// apps have migrated.

// experimental.identity.getAuthToken fetches an OAuth 2 function for
// the caller. The request has three sub-flows: non-interactive,
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
class ExperimentalIdentityGetAuthTokenFunction
    : public AsyncExtensionFunction,
      public ExtensionInstallPrompt::Delegate,
      public OAuth2MintTokenFlow::Delegate,
      public IdentitySigninFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN);

  ExperimentalIdentityGetAuthTokenFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~ExperimentalIdentityGetAuthTokenFunction();

 private:
  friend class ExperimentalGetAuthTokenFunctionTest;
  friend class ExperimentalMockGetAuthTokenFunction;

  // Helpers to report async function results to the caller.
  void CompleteFunctionWithResult(const std::string& access_token);
  void CompleteFunctionWithError(const std::string& error);

  // Initiate/complete the sub-flows.
  void StartSigninFlow();
  void StartMintTokenFlow(IdentityMintRequestQueue::MintType type);

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
  scoped_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  std::string refresh_token_;
  bool should_prompt_for_signin_;

  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  IssueAdviceInfo issue_advice_;
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_ptr<IdentitySigninFlow> signin_flow_;
};

class ExperimentalIdentityLaunchWebAuthFlowFunction
    : public AsyncExtensionFunction,
      public WebAuthFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.identity.launchWebAuthFlow",
                             EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW);
  ExperimentalIdentityLaunchWebAuthFlowFunction();

  // URL checking helpers. Public for testing.
  // Checks to see if the current URL ends the flow.
  bool IsFinalRedirectURL(const GURL& url) const;

  // Unit tests may override extension_id.
  void InitFinalRedirectURLPrefixesForTest(const std::string& extension_id);

 protected:
  virtual ~ExperimentalIdentityLaunchWebAuthFlowFunction();
  virtual bool RunImpl() OVERRIDE;

  // Helper to initialize final URLs vector.
  void InitFinalRedirectURLPrefixes(const std::string& extension_id);

  scoped_ptr<WebAuthFlow> auth_flow_;
  std::vector<GURL> final_prefixes_;

 private:
  // WebAuthFlow::Delegate implementation.
  virtual void OnAuthFlowFailure(WebAuthFlow::Failure failure) OVERRIDE;
  virtual void OnAuthFlowURLChange(const GURL& redirect_url) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_EXPERIMENTAL_IDENTITY_API_H_
