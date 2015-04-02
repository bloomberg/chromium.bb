// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/cryptauth_client_factory.h"

#include "components/proximity_auth/cryptauth/cryptauth_account_token_fetcher.h"

namespace proximity_auth {

CryptAuthClientFactory::CryptAuthClientFactory(
    OAuth2TokenService* token_service,
    const std::string& account_id,
    scoped_refptr<net::URLRequestContextGetter> url_request_context,
    const cryptauth::DeviceClassifier& device_classifier)
    : token_service_(token_service),
      account_id_(account_id),
      url_request_context_(url_request_context),
      device_classifier_(device_classifier) {
}

CryptAuthClientFactory::~CryptAuthClientFactory() {
}

scoped_ptr<CryptAuthClient> CryptAuthClientFactory::CreateInstance() {
  scoped_ptr<CryptAuthAccessTokenFetcher> access_token_fetcher(
      new CryptAuthAccountTokenFetcher(token_service_, account_id_));
  return make_scoped_ptr(new CryptAuthClient(
      access_token_fetcher.Pass(), url_request_context_, device_classifier_));
}

}  // namespace proximity_auth
