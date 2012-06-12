// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace {

const char kInvalidClientId[] = "Invalid OAuth2 Client ID.";
const char kInvalidScopes[] = "Invalid OAuth2 scopes.";
const char kInvalidRedirect[] = "Did not redirect to the right URL.";

}  // namespace

GetAuthTokenFunction::GetAuthTokenFunction() {}
GetAuthTokenFunction::~GetAuthTokenFunction() {}

bool GetAuthTokenFunction::RunImpl() {
  const Extension* extension = GetExtension();
  Extension::OAuth2Info oauth2_info = extension->oauth2_info();

  if (oauth2_info.client_id.empty()) {
    error_ = kInvalidClientId;
    return false;
  }

  if (oauth2_info.scopes.size() == 0) {
    error_ = kInvalidScopes;
    return false;
  }

  AddRef();  // Balanced in OnMintTokenSuccess|Failure.

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile());

  flow_.reset(new OAuth2MintTokenFlow(
      profile()->GetRequestContext(),
      this,
      OAuth2MintTokenFlow::Parameters(
          token_service->GetOAuth2LoginRefreshToken(),
          extension->id(),
          oauth2_info.client_id,
          oauth2_info.scopes,
          OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE)));
  flow_->Start();

  return true;
}

void GetAuthTokenFunction::OnMintTokenSuccess(const std::string& access_token) {
  result_.reset(Value::CreateStringValue(access_token));
  SendResponse(true);
  Release();  // Balanced in RunImpl.
}

void GetAuthTokenFunction::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  error_ = error.ToString();
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

LaunchWebAuthFlowFunction::LaunchWebAuthFlowFunction() {}
LaunchWebAuthFlowFunction::~LaunchWebAuthFlowFunction() {}

bool LaunchWebAuthFlowFunction::RunImpl() {
  DictionaryValue* arg = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &arg));

  std::string url;
  EXTENSION_FUNCTION_VALIDATE(arg->GetString("url", &url));

  bool interactive = false;
  arg->GetBoolean("interactive", &interactive);

  WebAuthFlow::Mode mode = interactive ?
      WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT;

  AddRef();  // Balanced in OnAuthFlowSuccess/Failure.
  GURL auth_url(url);
  auth_flow_.reset(new WebAuthFlow(
      this, profile(), GetExtension()->id(), auth_url, mode));
  auth_flow_->Start();
  return true;
}

void LaunchWebAuthFlowFunction::OnAuthFlowSuccess(
    const std::string& redirect_url) {
  result_.reset(Value::CreateStringValue(redirect_url));
  SendResponse(true);
  Release();  // Balanced in RunImpl.
}

void LaunchWebAuthFlowFunction::OnAuthFlowFailure() {
  error_ = kInvalidRedirect;
  SendResponse(false);
  Release();  // Balanced in RunImpl.
}

}  // namespace extensions
