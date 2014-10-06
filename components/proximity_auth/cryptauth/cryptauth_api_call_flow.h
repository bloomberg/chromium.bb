// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_API_CALL_FLOW_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_API_CALL_FLOW_H

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"

namespace proximity_auth {

// Google API call flow implementation underlying all CryptAuth API calls.
// CryptAuth is a Google service that manages authorization and cryptographic
// credentials for users' devices (eg. public keys).
class CryptAuthApiCallFlow : public OAuth2ApiCallFlow {
 public:
  typedef base::Callback<void(const std::string& serialized_response)>
      ResultCallback;
  typedef base::Callback<void(const std::string& error_message)> ErrorCallback;

  // Creates a flow that can make a CryptAuth request to the URL given by
  // |request_url|. If the |access_token| is empty or invalid, a new
  // token will be minted to make the API call using |refresh_token|.
  // The |context| provided here should outlive this flow.
  CryptAuthApiCallFlow(net::URLRequestContextGetter* context,
                       const std::string& refresh_token,
                       const std::string& access_token,
                       const GURL& request_url);
  virtual ~CryptAuthApiCallFlow();

  // Sends |serialized_request|, which should be a serialized protocol buffer
  // message. Upon success |result_callback| will be called with the serialized
  // response protocol buffer message. Upon failure, |error_callback| will be
  // called.
  void Start(const std::string& serialized_request,
             ResultCallback result_callback,
             ErrorCallback error_callback);

 protected:
  // Reduce the visibility of OAuth2ApiCallFlow::Start() to avoid exposing
  // overloaded methods.
  using OAuth2ApiCallFlow::Start;

  // google_apis::OAuth2ApiCallFlow:
  virtual GURL CreateApiCallUrl() OVERRIDE;
  virtual std::string CreateApiCallBody() OVERRIDE;
  virtual std::string CreateApiCallBodyContentType() OVERRIDE;
  virtual void ProcessApiCallSuccess(const net::URLFetcher* source) OVERRIDE;
  virtual void ProcessApiCallFailure(const net::URLFetcher* source) OVERRIDE;
  virtual void ProcessNewAccessToken(const std::string& access_token) OVERRIDE;
  virtual void ProcessMintAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

 private:
  // The URL of the CryptAuth endpoint serving the request.
  const GURL request_url_;

  // Serialized request message proto that will be sent in the API request.
  std::string serialized_request_;

  // Callback invoked with the serialized response message proto when the flow
  // completes successfully.
  ResultCallback result_callback_;

  // Callback invoked with an error message when the flow fails.
  ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthApiCallFlow);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_API_CALL_FLOW_H
