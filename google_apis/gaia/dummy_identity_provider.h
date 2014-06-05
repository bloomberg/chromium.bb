// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_DUMMY_IDENTITY_PROVIDER_H_
#define GOOGLE_APIS_GAIA_DUMMY_IDENTITY_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/identity_provider.h"

// GCMDriverDesktop depends on IdentityProvider as it was originally meant to
// work for signed-in users only. This dependency is being removed. While it is
// still present, DummyIdentityProvider will be used for the browser-global
// GCMDriver instance. The DummyIdentityProvider never has any active user or
// tokens.
// TODO(bartfab,jianli): Remove this class when GCMDriverDesktop no longer
// depends on IdentityService.
class DummyIdentityProvider : public IdentityProvider {
 public:
  DummyIdentityProvider();
  virtual ~DummyIdentityProvider();

  // IdentityProvider:
  virtual std::string GetActiveUsername() OVERRIDE;
  virtual std::string GetActiveAccountId() OVERRIDE;
  virtual OAuth2TokenService* GetTokenService() OVERRIDE;
  virtual bool RequestLogin() OVERRIDE;

 private:
  class DummyOAuth2TokenService;

  scoped_ptr<DummyOAuth2TokenService> oauth2_token_service_;

  DISALLOW_COPY_AND_ASSIGN(DummyIdentityProvider);
};

#endif  // GOOGLE_APIS_GAIA_FAKE_IDENTITY_PROVIDER_H_
