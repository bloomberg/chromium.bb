// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/app_notify_channel_setup.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/net/gaia/oauth2_mint_token_flow.h"

class GoogleServiceAuthError;

namespace extensions {

class GetAuthTokenFunction : public AsyncExtensionFunction,
                             public OAuth2MintTokenFlow::Delegate,
                             public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.identity.getAuthToken");

  GetAuthTokenFunction();

 private:
  virtual ~GetAuthTokenFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // OAuth2MintTokenFlow::Delegate implementation:
  virtual void OnMintTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnMintTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnIssueAdviceSuccess(
      const IssueAdviceInfo& issue_advice) OVERRIDE;

  // ExtensionInstallPrompt::Delegate implementation:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Starts a MintTokenFlow with the given mode; Returns success.
  bool StartFlow(OAuth2MintTokenFlow::Mode mode);

  bool interactive_;
  scoped_ptr<OAuth2MintTokenFlow> flow_;

  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
};

class LaunchWebAuthFlowFunction : public AsyncExtensionFunction,
                                  public WebAuthFlow::Delegate {
 public:
  LaunchWebAuthFlowFunction();

 private:
  virtual ~LaunchWebAuthFlowFunction();
  virtual bool RunImpl() OVERRIDE;

  // WebAuthFlow::Delegate implementation.
  virtual void OnAuthFlowSuccess(const std::string& redirect_url) OVERRIDE;
  virtual void OnAuthFlowFailure() OVERRIDE;

  scoped_ptr<WebAuthFlow> auth_flow_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.identity.launchWebAuthFlow");
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
