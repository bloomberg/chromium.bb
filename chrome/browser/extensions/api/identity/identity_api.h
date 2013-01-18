// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/app_notify_channel_setup.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

class GetAuthTokenFunctionTest;
class MockGetAuthTokenFunction;
class GoogleServiceAuthError;

namespace extensions {

namespace identity_constants {
extern const char kInvalidClientId[];
extern const char kInvalidScopes[];
extern const char kAuthFailure[];
extern const char kNoGrant[];
extern const char kUserRejected[];
extern const char kUserNotSignedIn[];
extern const char kInvalidRedirect[];
}

class IdentityGetAuthTokenFunction : public AsyncExtensionFunction,
                                     public OAuth2MintTokenFlow::Delegate,
                                     public ExtensionInstallPrompt::Delegate,
                                     public LoginUIService::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN)

  IdentityGetAuthTokenFunction();

 protected:
  virtual ~IdentityGetAuthTokenFunction();

 private:
  friend class ::GetAuthTokenFunctionTest;
  friend class ::MockGetAuthTokenFunction;

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // OAuth2MintTokenFlow::Delegate implementation:
  virtual void OnMintTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnMintTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnIssueAdviceSuccess(
      const IssueAdviceInfo& issue_advice) OVERRIDE;

  // LoginUIService::Observer implementation.
  virtual void OnLoginUIShown(LoginUIService::LoginUI* ui) OVERRIDE {
    // Do nothing when login ui is shown.
  }
  virtual void OnLoginUIClosed(LoginUIService::LoginUI* ui) OVERRIDE;

  // ExtensionInstallPrompt::Delegate implementation:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Shows the login UI in a browser popup.
  bool StartLogin();
  // Starts a MintTokenFlow with the given mode; Returns success.
  bool StartFlow(OAuth2MintTokenFlow::Mode mode);

  virtual void StartObservingLoginService();
  virtual void StopObservingLoginService();
  virtual void ShowLoginPopup();
  virtual void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice);
  // Caller owns the returned instance.
  virtual OAuth2MintTokenFlow* CreateMintTokenFlow(
      OAuth2MintTokenFlow::Mode mode);

  // Checks if there is a master login token to mint tokens for the extension.
  virtual bool HasLoginToken() const;

  bool interactive_;
  scoped_ptr<OAuth2MintTokenFlow> flow_;

  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
};

class IdentityLaunchWebAuthFlowFunction : public AsyncExtensionFunction,
                                          public WebAuthFlow::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.identity.launchWebAuthFlow",
                             EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW)

  IdentityLaunchWebAuthFlowFunction();

 private:
  virtual ~IdentityLaunchWebAuthFlowFunction();
  virtual bool RunImpl() OVERRIDE;

  // WebAuthFlow::Delegate implementation.
  virtual void OnAuthFlowSuccess(const std::string& redirect_url) OVERRIDE;
  virtual void OnAuthFlowFailure() OVERRIDE;

  scoped_ptr<WebAuthFlow> auth_flow_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
