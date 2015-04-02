// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_CLIENT_FACTORY_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_CLIENT_FACTORY_H

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace proximity_auth {

class CryptAuthClient;

// Factory for creating CryptAuthClient instances. Because each CryptAuthClient
// instance can only be used for one API call, this class helps making multiple
// requests in sequence or parallel easier.
class CryptAuthClientFactory {
 public:
  // Creates the factory.
  // |token_service|: Gets the user's access token.
  //     Needs to outlive this object.
  // |account_id|: The account id of the user.
  // |url_request_context|: The request context to make the HTTP requests.
  // |device_classifier|: Contains basic device information of the client.
  CryptAuthClientFactory(
      OAuth2TokenService* token_service,
      const std::string& account_id,
      scoped_refptr<net::URLRequestContextGetter> url_request_context,
      const cryptauth::DeviceClassifier& device_classifier);
  virtual ~CryptAuthClientFactory();

  // Creates and returns a CryptAuthClient instance that is owned by the caller.
  scoped_ptr<CryptAuthClient> CreateInstance();

 private:
  OAuth2TokenService* token_service_;
  const std::string account_id_;
  const scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  const cryptauth::DeviceClassifier device_classifier_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthClientFactory);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_CLIENT_FACTORY_H
