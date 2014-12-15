// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/identity/identity_api.h"

#include <set>
#include <string>

#include "extensions/common/manifest_handlers/oauth2_manifest_handler.h"
#include "extensions/shell/browser/shell_oauth2_token_service.h"
#include "extensions/shell/common/api/identity.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace extensions {
namespace shell {

namespace {
const char kIdentityApiId[] = "identity_api";
const char kErrorNoUserAccount[] = "No user account.";
const char kErrorNoRefreshToken[] = "No refresh token.";
const char kErrorNoScopesInManifest[] = "No scopes in manifest.";
}  // namespace

IdentityGetAuthTokenFunction::IdentityGetAuthTokenFunction()
    : OAuth2TokenService::Consumer(kIdentityApiId) {
}

IdentityGetAuthTokenFunction::~IdentityGetAuthTokenFunction() {
}

ExtensionFunction::ResponseAction IdentityGetAuthTokenFunction::Run() {
  scoped_ptr<api::identity::GetAuthToken::Params> params(
      api::identity::GetAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ShellOAuth2TokenService* service = ShellOAuth2TokenService::GetInstance();
  std::string account_id = service->account_id();
  if (account_id.empty())
    return RespondNow(Error(kErrorNoUserAccount));

  if (!service->RefreshTokenIsAvailable(account_id))
    return RespondNow(Error(kErrorNoRefreshToken));

  // Verify that we have scopes.
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension());
  std::set<std::string> scopes(oauth2_info.scopes.begin(),
                               oauth2_info.scopes.end());
  if (scopes.empty())
    return RespondNow(Error(kErrorNoScopesInManifest));

  AddRef();  // Balanced in OnGetTokenSuccess() and OnGetTokenFailure().

  // Start an asynchronous token fetch.
  access_token_request_ =
      service->StartRequest(service->account_id(), scopes, this);
  return RespondLater();
}

void IdentityGetAuthTokenFunction::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  Respond(OneArgument(new base::StringValue(access_token)));
  Release();  // Balanced in Run().
}

void IdentityGetAuthTokenFunction::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  Respond(Error(error.ToString()));
  Release();  // Balanced in Run().
}

///////////////////////////////////////////////////////////////////////////////

IdentityRemoveCachedAuthTokenFunction::IdentityRemoveCachedAuthTokenFunction() {
}

IdentityRemoveCachedAuthTokenFunction::
    ~IdentityRemoveCachedAuthTokenFunction() {
}

ExtensionFunction::ResponseAction IdentityRemoveCachedAuthTokenFunction::Run() {
  scoped_ptr<api::identity::RemoveCachedAuthToken::Params> params(
      api::identity::RemoveCachedAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  // This stub identity API does not maintain a token cache, so there is nothing
  // to remove.
  return RespondNow(NoArguments());
}

}  // namespace shell
}  // namespace extensions
