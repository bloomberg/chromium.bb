// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_CLIENT_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_CLIENT_H

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

class OAuth2TokenService;

namespace proximity_auth {

class CryptAuthAccessTokenFetcher;
class CryptAuthApiCallFlow;

// Use CryptAuthClient to make API requests to the CryptAuth service, which
// manages cryptographic credentials (ie. public keys) for a user's devices.
// CryptAuthClient only processes one request, so create a new instance for each
// request you make. DO NOT REUSE.
// For documentation on each API call, see
// components/proximity_auth/cryptauth/proto/cryptauth_api.proto
class CryptAuthClient {
 public:
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  // Creates the client using |url_request_context| to make the HTTP request.
  // CryptAuthClient takes ownership of |access_token_fetcher|, which provides
  // the access token authorizing CryptAuth requests.
  CryptAuthClient(
      scoped_ptr<CryptAuthAccessTokenFetcher> access_token_fetcher,
      scoped_refptr<net::URLRequestContextGetter> url_request_context);
  virtual ~CryptAuthClient();

  // GetMyDevices
  typedef base::Callback<void(const cryptauth::GetMyDevicesResponse&)>
      GetMyDevicesCallback;
  void GetMyDevices(const cryptauth::GetMyDevicesRequest& request,
                    const GetMyDevicesCallback& callback,
                    const ErrorCallback& error_callback);

  // FindEligibleUnlockDevices
  typedef base::Callback<void(
      const cryptauth::FindEligibleUnlockDevicesResponse&)>
      FindEligibleUnlockDevicesCallback;
  void FindEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesRequest& request,
      const FindEligibleUnlockDevicesCallback& callback,
      const ErrorCallback& error_callback);

  // SendDeviceSyncTickle
  typedef base::Callback<void(const cryptauth::SendDeviceSyncTickleResponse&)>
      SendDeviceSyncTickleCallback;
  void SendDeviceSyncTickle(
      const cryptauth::SendDeviceSyncTickleRequest& request,
      const SendDeviceSyncTickleCallback& callback,
      const ErrorCallback& error_callback);

  // ToggleEasyUnlock
  typedef base::Callback<void(const cryptauth::ToggleEasyUnlockResponse&)>
      ToggleEasyUnlockCallback;
  void ToggleEasyUnlock(const cryptauth::ToggleEasyUnlockRequest& request,
                        const ToggleEasyUnlockCallback& callback,
                        const ErrorCallback& error_callback);

  // SetupEnrollment
  typedef base::Callback<void(const cryptauth::SetupEnrollmentResponse&)>
      SetupEnrollmentCallback;
  void SetupEnrollment(const cryptauth::SetupEnrollmentRequest& request,
                       const SetupEnrollmentCallback& callback,
                       const ErrorCallback& error_callback);

  // FinishEnrollment
  typedef base::Callback<void(const cryptauth::FinishEnrollmentResponse&)>
      FinishEnrollmentCallback;
  void FinishEnrollment(const cryptauth::FinishEnrollmentRequest& request,
                        const FinishEnrollmentCallback& callback,
                        const ErrorCallback& error_callback);

 protected:
  // Creates a CryptAuthApiCallFlow object. Exposed for testing.
  virtual scoped_ptr<CryptAuthApiCallFlow> CreateFlow(const GURL& request_url);

 private:
  // Starts a call to the API given by |request_path|, with the templated
  // request and response types. The client first fetches the access token and
  // then makes the HTTP request.
  template <class RequestProto, class ResponseProto>
  void MakeApiCall(
      const std::string& request_path,
      const RequestProto& request_proto,
      const base::Callback<void(const ResponseProto&)>& response_callback,
      const ErrorCallback& error_callback);

  // Called when the access token is obtained so the API request can be made.
  template <class ResponseProto>
  void OnAccessTokenFetched(
      const std::string& serialized_request,
      const base::Callback<void(const ResponseProto&)>& response_callback,
      const std::string& access_token);

  // Called with CryptAuthApiCallFlow completes successfully to deserialize and
  // return the result.
  template <class ResponseProto>
  void OnFlowSuccess(
      const base::Callback<void(const ResponseProto&)>& result_callback,
      const std::string& serialized_response);

  // Called when the current API call fails at any step.
  void OnApiCallFailed(const std::string& error_message);

  // The context for network requests.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;

  // Fetches the access token authorizing the API calls.
  scoped_ptr<CryptAuthAccessTokenFetcher> access_token_fetcher_;

  // Handles the current API call.
  scoped_ptr<CryptAuthApiCallFlow> flow_;

  // URL path of the current request.
  std::string request_path_;

  // Called when the current request fails.
  ErrorCallback error_callback_;

  base::WeakPtrFactory<CryptAuthClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthClient);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPT_AUTH_CLIENT_H
