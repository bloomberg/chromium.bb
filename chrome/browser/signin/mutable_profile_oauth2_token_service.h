// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include "chrome/browser/signin/profile_oauth2_token_service.h"

// A specialization of ProfileOAuth2TokenService that can can mutate its OAuth2
// tokens.
//
// Note: This class is just a placeholder for now. Methods used to mutate
// the tokens are currently being migrated from ProfileOAuth2TokenService.
class MutableProfileOAuth2TokenService : public ProfileOAuth2TokenService {
 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  MutableProfileOAuth2TokenService();
  virtual ~MutableProfileOAuth2TokenService();

 private:
  DISALLOW_COPY_AND_ASSIGN(MutableProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
