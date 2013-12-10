// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"

// A specialization of ProfileOAuth2TokenService that can can mutate its OAuth2
// tokens.
//
// Note: This class is just a placeholder for now. Methods used to mutate
// the tokens are currently being migrated from ProfileOAuth2TokenService.
class MutableProfileOAuth2TokenService : public ProfileOAuth2TokenService,
                                         public WebDataServiceConsumer  {
 public:
  // ProfileOAuth2TokenService.
  virtual void Shutdown() OVERRIDE;
  virtual void LoadCredentials() OVERRIDE;

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  MutableProfileOAuth2TokenService();
  virtual ~MutableProfileOAuth2TokenService();

 private:
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceTest,
                           TokenServiceUpdateClearsCache);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceTest,
                           PersistenceDBUpgrade);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceTest,
                           PersistenceLoadCredentials);

  // WebDataServiceConsumer implementation:
  virtual void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      const WDTypedResult* result) OVERRIDE;

  // When migrating an old login-scoped refresh token, this returns the account
  // ID with which the token was associated.
  std::string GetAccountIdForMigratingRefreshToken();

  // Loads credentials into in memory stucture.
  void LoadAllCredentialsIntoMemory(
      const std::map<std::string, std::string>& db_tokens);

  // Handle to the request reading tokens from database.
  WebDataServiceBase::Handle web_data_service_request_;

  DISALLOW_COPY_AND_ASSIGN(MutableProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_H_
