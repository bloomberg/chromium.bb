// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_UI_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/ui/webui/identity_internals/identity_internals.mojom.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"

class Profile;
class IdentityInternalsTokenRevoker;

// Class acting as a controller of the chrome://identity-internals WebUI.
class IdentityInternalsUIHandler
    : public mojo::InterfaceImpl<IdentityInternalsHandlerMojo>,
      public MojoWebUIHandler {
 public:
  explicit IdentityInternalsUIHandler(Profile* profile);
  virtual ~IdentityInternalsUIHandler();

  // Ensures that a proper clean up happens after a token is revoked. That
  // includes deleting the |token_revoker|, removing the token from Identity API
  // cache and updating the UI that the token is gone.
  void OnTokenRevokerDone(IdentityInternalsTokenRevoker* token_revoker);

  // Overridden from IdentityInternalsHandlerMojo:
  virtual void GetTokens(
      const mojo::Callback<void(mojo::Array<IdentityTokenMojoPtr>)>& callback)
      OVERRIDE;
  virtual void RevokeToken(const mojo::String& extension_id,
                           const mojo::String& access_token,
                           const mojo::Callback<void()>& callback) OVERRIDE;

 private:
  // We use an explicit conversion function instead of TypeConverter because
  // creating an IdentityTokenMojo relies on having the Profile* as state.
  mojo::Array<IdentityTokenMojoPtr> ConvertCachedTokens(
      const extensions::IdentityAPI::CachedTokens& tokens);

  // Gets the name of an extension referred to by |token_cache_key| as a string.
  const std::string GetExtensionName(
      const extensions::ExtensionTokenKey& token_cache_key);

  // Gets a list of scopes specified in |token_cache_key| and returns a pointer
  // to a ListValue containing the scopes. The caller gets ownership of the
  // returned object.
  mojo::Array<mojo::String> GetScopes(
      const extensions::ExtensionTokenKey& token_cache_key);

  // Gets a localized status of the access token in |token_cache_value|.
  const std::string GetStatus(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  // Gets a string representation of an expiration time of the access token in
  // |token_cache_value|.
  const std::string GetExpirationTime(
      const extensions::IdentityTokenCacheValue& token_cache_value);

  Profile* profile_;

  // A vector of token revokers that are currently revoking tokens.
  ScopedVector<IdentityInternalsTokenRevoker> token_revokers_;

  DISALLOW_COPY_AND_ASSIGN(IdentityInternalsUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_UI_HANDLER_H_
