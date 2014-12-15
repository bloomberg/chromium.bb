// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_IDENTITY_IDENTITY_API_H_
#define EXTENSIONS_SHELL_BROWSER_API_IDENTITY_IDENTITY_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace extensions {
namespace shell {

// Returns an OAuth2 access token for a user. See the IDL file for
// documentation.
class IdentityGetAuthTokenFunction : public UIThreadExtensionFunction,
                                     public OAuth2TokenService::Consumer {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAuthToken", UNKNOWN);

  IdentityGetAuthTokenFunction();

 protected:
  ~IdentityGetAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // OAuth2TokenService::Consumer:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

 private:
  // A pending token fetch request.
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(IdentityGetAuthTokenFunction);
};

// Stub. See the IDL file for documentation.
class IdentityRemoveCachedAuthTokenFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.removeCachedAuthToken", UNKNOWN)

  IdentityRemoveCachedAuthTokenFunction();

 protected:
  ~IdentityRemoveCachedAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdentityRemoveCachedAuthTokenFunction);
};

}  // namespace shell
}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_IDENTITY_IDENTITY_API_H_
