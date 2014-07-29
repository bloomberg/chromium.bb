// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_TOKEN_REVOKER_H_
#define CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_TOKEN_REVOKER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "mojo/public/cpp/bindings/callback.h"

class IdentityInternalsUIHandler;
class Profile;

// Handles the revoking of an access token and helps performing the clean up
// after it is revoked by holding information about the access token and related
// extension ID.
class IdentityInternalsTokenRevoker : public GaiaAuthConsumer {
 public:
  // Revokes |access_token| from extension with |extension_id|.
  // |profile| is required for its request context. |consumer| will be
  // notified when revocation succeeds via |OnTokenRevokerDone()|.
  IdentityInternalsTokenRevoker(const std::string& extension_id,
                                const std::string& access_token,
                                const mojo::Callback<void()>& callback,
                                Profile* profile,
                                IdentityInternalsUIHandler* consumer);
  virtual ~IdentityInternalsTokenRevoker();

  // Returns the access token being revoked.
  const std::string& access_token() const { return access_token_; }

  // Returns the ID of the extension the access token is related to.
  const std::string& extension_id() const { return extension_id_; }

  const mojo::Callback<void()>& callback() const { return callback_; }

  // GaiaAuthConsumer implementation.
  virtual void OnOAuth2RevokeTokenCompleted() OVERRIDE;

 private:
  // An object used to start a token revoke request.
  GaiaAuthFetcher fetcher_;

  // An ID of an extension the access token is related to.
  const std::string extension_id_;

  // The access token to revoke.
  const std::string access_token_;

  // Callback for when we complete.
  const mojo::Callback<void()> callback_;

  // An object that needs to be notified once the access token is revoked.
  IdentityInternalsUIHandler* consumer_;  // weak.

  DISALLOW_COPY_AND_ASSIGN(IdentityInternalsTokenRevoker);
};

#endif  // CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_TOKEN_REVOKER_H_
