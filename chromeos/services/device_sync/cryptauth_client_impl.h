// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/device_sync/cryptauth_api_call_flow.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/access_token_info.h"

namespace identity {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace identity

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class GoogleServiceAuthError;

namespace chromeos {

namespace device_sync {

// Implementation of CryptAuthClient.
// Note: There is no need to set the |device_classifier| field in request
// messages. CryptAuthClient will fill this field for all requests.
class CryptAuthClientImpl : public CryptAuthClient {
 public:
  // Creates the client using |url_request_context| to make the HTTP request
  // through |api_call_flow|. The |device_classifier| argument contains basic
  // device information of the caller (e.g. version and device type).
  // TODO(nohle): Remove the |device_classifier| argument when the CryptAuth v1
  // methods are no longer needed.
  CryptAuthClientImpl(
      std::unique_ptr<CryptAuthApiCallFlow> api_call_flow,
      identity::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const cryptauth::DeviceClassifier& device_classifier);
  ~CryptAuthClientImpl() override;

  // CryptAuthClient:
  void GetMyDevices(const cryptauth::GetMyDevicesRequest& request,
                    const GetMyDevicesCallback& callback,
                    const ErrorCallback& error_callback,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation) override;
  void FindEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesRequest& request,
      const FindEligibleUnlockDevicesCallback& callback,
      const ErrorCallback& error_callback) override;
  void FindEligibleForPromotion(
      const cryptauth::FindEligibleForPromotionRequest& request,
      const FindEligibleForPromotionCallback& callback,
      const ErrorCallback& error_callback) override;
  void SendDeviceSyncTickle(
      const cryptauth::SendDeviceSyncTickleRequest& request,
      const SendDeviceSyncTickleCallback& callback,
      const ErrorCallback& error_callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override;
  void ToggleEasyUnlock(const cryptauth::ToggleEasyUnlockRequest& request,
                        const ToggleEasyUnlockCallback& callback,
                        const ErrorCallback& error_callback) override;
  void SetupEnrollment(const cryptauth::SetupEnrollmentRequest& request,
                       const SetupEnrollmentCallback& callback,
                       const ErrorCallback& error_callback) override;
  void FinishEnrollment(const cryptauth::FinishEnrollmentRequest& request,
                        const FinishEnrollmentCallback& callback,
                        const ErrorCallback& error_callback) override;
  void SyncKeys(const cryptauthv2::SyncKeysRequest& request,
                const SyncKeysCallback& callback,
                const ErrorCallback& error_callback) override;
  void EnrollKeys(const cryptauthv2::EnrollKeysRequest& request,
                  const EnrollKeysCallback& callback,
                  const ErrorCallback& error_callback) override;
  std::string GetAccessTokenUsed() override;

 private:
  // Starts a call to the API given by |request_url|, with the templated
  // request and response types. The client first fetches the access token and
  // then makes the HTTP request.
  template <class RequestProto, class ResponseProto>
  void MakeApiCall(
      const GURL& request_url,
      const RequestProto& request_proto,
      const base::Callback<void(const ResponseProto&)>& response_callback,
      const ErrorCallback& error_callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation);

  // Called when the access token is obtained so the API request can be made.
  template <class ResponseProto>
  void OnAccessTokenFetched(
      const std::string& serialized_request,
      const base::Callback<void(const ResponseProto&)>& response_callback,
      GoogleServiceAuthError error,
      identity::AccessTokenInfo access_token_info);

  // Called with CryptAuthApiCallFlow completes successfully to deserialize and
  // return the result.
  template <class ResponseProto>
  void OnFlowSuccess(
      const base::Callback<void(const ResponseProto&)>& result_callback,
      const std::string& serialized_response);

  // Called when the current API call fails at any step.
  void OnApiCallFailed(NetworkRequestError error);

  // Returns a copy of the input request with the device classifier field set.
  // Only used for CryptAuth v1 protos.
  template <class RequestProto>
  RequestProto RequestWithDeviceClassifierSet(const RequestProto& request);

  // Constructs and executes the actual HTTP request.
  std::unique_ptr<CryptAuthApiCallFlow> api_call_flow_;

  identity::IdentityManager* identity_manager_;

  // Fetches the access token authorizing the API calls.
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The context for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Contains basic device info of the client making the request that is sent to
  // CryptAuth with each API call.
  const cryptauth::DeviceClassifier device_classifier_;

  // True if an API call has been started. Remains true even after the API call
  // completes.
  bool has_call_started_;

  // URL of the current request.
  GURL request_url_;

  // The access token fetched by |access_token_fetcher_|.
  std::string access_token_used_;

  // Called when the current request fails.
  ErrorCallback error_callback_;

  base::WeakPtrFactory<CryptAuthClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthClientImpl);
};

// Implementation of CryptAuthClientFactory.
class CryptAuthClientFactoryImpl : public CryptAuthClientFactory {
 public:
  // |identity_manager|: Gets the user's access token.
  //     Not owned, so |identity_manager| needs to outlive this object.
  // |url_request_context|: The request context to make the HTTP requests.
  // |device_classifier|: Contains basic device information of the client.
  CryptAuthClientFactoryImpl(
      identity::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const cryptauth::DeviceClassifier& device_classifier);
  ~CryptAuthClientFactoryImpl() override;

  // CryptAuthClientFactory:
  std::unique_ptr<CryptAuthClient> CreateInstance() override;

 private:
  identity::IdentityManager* identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const cryptauth::DeviceClassifier device_classifier_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthClientFactoryImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_IMPL_H_
