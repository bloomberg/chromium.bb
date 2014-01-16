// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_SERVICE_H_
#define GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace net {
class URLRequestContextGetter;
}

// Do-nothing implementation of OAuth2TokenService.
class FakeOAuth2TokenService : public OAuth2TokenService {
 public:
  FakeOAuth2TokenService();
  virtual ~FakeOAuth2TokenService();

 protected:
  // OAuth2TokenService overrides.
  virtual void FetchOAuth2Token(RequestImpl* request,
                                const std::string& account_id,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes) OVERRIDE;

  virtual void InvalidateOAuth2Token(const std::string& account_id,
                                     const std::string& client_id,
                                     const ScopeSet& scopes,
                                     const std::string& access_token) OVERRIDE;

  virtual std::string GetRefreshToken(const std::string& account_id) OVERRIDE;

 private:
  // OAuth2TokenService overrides.
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FakeOAuth2TokenService);
};

#endif  // GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_SERVICE_H_
