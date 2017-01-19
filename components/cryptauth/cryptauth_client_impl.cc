// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/cryptauth_client_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "components/cryptauth/cryptauth_access_token_fetcher_impl.h"
#include "components/cryptauth/switches.h"

namespace cryptauth {

namespace {

// Default URL of Google APIs endpoint hosting CryptAuth.
const char kDefaultCryptAuthHTTPHost[] = "https://www.googleapis.com";

// URL subpath hosting the CryptAuth service.
const char kCryptAuthPath[] = "cryptauth/v1/";

// URL subpaths for each CryptAuth API.
const char kGetMyDevicesPath[] = "deviceSync/getmydevices";
const char kFindEligibleUnlockDevicesPath[] =
    "deviceSync/findeligibleunlockdevices";
const char kSendDeviceSyncTicklePath[] = "deviceSync/senddevicesynctickle";
const char kToggleEasyUnlockPath[] = "deviceSync/toggleeasyunlock";
const char kSetupEnrollmentPath[] = "enrollment/setup";
const char kFinishEnrollmentPath[] = "enrollment/finish";

// Query string of the API URL indicating that the response should be in a
// serialized protobuf format.
const char kQueryProtobuf[] = "?alt=proto";

// Creates the full CryptAuth URL for endpoint to the API with |request_path|.
GURL CreateRequestUrl(const std::string& request_path) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GURL google_apis_url =
      GURL(command_line->HasSwitch(switches::kCryptAuthHTTPHost)
               ? command_line->GetSwitchValueASCII(switches::kCryptAuthHTTPHost)
               : kDefaultCryptAuthHTTPHost);
  return google_apis_url.Resolve(kCryptAuthPath + request_path +
                                 kQueryProtobuf);
}

}  // namespace

CryptAuthClientImpl::CryptAuthClientImpl(
    std::unique_ptr<CryptAuthApiCallFlow> api_call_flow,
    std::unique_ptr<CryptAuthAccessTokenFetcher> access_token_fetcher,
    scoped_refptr<net::URLRequestContextGetter> url_request_context,
    const DeviceClassifier& device_classifier)
    : api_call_flow_(std::move(api_call_flow)),
      access_token_fetcher_(std::move(access_token_fetcher)),
      url_request_context_(url_request_context),
      device_classifier_(device_classifier),
      has_call_started_(false),
      weak_ptr_factory_(this) {}

CryptAuthClientImpl::~CryptAuthClientImpl() {
}

void CryptAuthClientImpl::GetMyDevices(
    const GetMyDevicesRequest& request,
    const GetMyDevicesCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kGetMyDevicesPath, request, callback, error_callback);
}

void CryptAuthClientImpl::FindEligibleUnlockDevices(
    const FindEligibleUnlockDevicesRequest& request,
    const FindEligibleUnlockDevicesCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kFindEligibleUnlockDevicesPath, request, callback,
              error_callback);
}

void CryptAuthClientImpl::SendDeviceSyncTickle(
    const SendDeviceSyncTickleRequest& request,
    const SendDeviceSyncTickleCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kSendDeviceSyncTicklePath, request, callback, error_callback);
}

void CryptAuthClientImpl::ToggleEasyUnlock(
    const ToggleEasyUnlockRequest& request,
    const ToggleEasyUnlockCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kToggleEasyUnlockPath, request, callback, error_callback);
}

void CryptAuthClientImpl::SetupEnrollment(
    const SetupEnrollmentRequest& request,
    const SetupEnrollmentCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kSetupEnrollmentPath, request, callback, error_callback);
}

void CryptAuthClientImpl::FinishEnrollment(
    const FinishEnrollmentRequest& request,
    const FinishEnrollmentCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kFinishEnrollmentPath, request, callback, error_callback);
}

std::string CryptAuthClientImpl::GetAccessTokenUsed() {
  return access_token_used_;
}

template <class RequestProto, class ResponseProto>
void CryptAuthClientImpl::MakeApiCall(
    const std::string& request_path,
    const RequestProto& request_proto,
    const base::Callback<void(const ResponseProto&)>& response_callback,
    const ErrorCallback& error_callback) {
  if (has_call_started_) {
    error_callback.Run(
        "Client has been used for another request. Do not reuse.");
    return;
  }
  has_call_started_ = true;

  // The |device_classifier| field must be present for all CryptAuth requests.
  RequestProto request_copy(request_proto);
  request_copy.mutable_device_classifier()->CopyFrom(device_classifier_);

  std::string serialized_request;
  if (!request_copy.SerializeToString(&serialized_request)) {
    error_callback.Run(std::string("Failed to serialize ") +
                       request_proto.GetTypeName() + " proto.");
    return;
  }

  request_path_ = request_path;
  error_callback_ = error_callback;
  access_token_fetcher_->FetchAccessToken(base::Bind(
      &CryptAuthClientImpl::OnAccessTokenFetched<ResponseProto>,
      weak_ptr_factory_.GetWeakPtr(), serialized_request, response_callback));
}

template <class ResponseProto>
void CryptAuthClientImpl::OnAccessTokenFetched(
    const std::string& serialized_request,
    const base::Callback<void(const ResponseProto&)>& response_callback,
    const std::string& access_token) {
  if (access_token.empty()) {
    OnApiCallFailed("Failed to get a valid access token.");
    return;
  }
  access_token_used_ = access_token;

  api_call_flow_->Start(
      CreateRequestUrl(request_path_), url_request_context_.get(), access_token,
      serialized_request,
      base::Bind(&CryptAuthClientImpl::OnFlowSuccess<ResponseProto>,
                 weak_ptr_factory_.GetWeakPtr(), response_callback),
      base::Bind(&CryptAuthClientImpl::OnApiCallFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

template <class ResponseProto>
void CryptAuthClientImpl::OnFlowSuccess(
    const base::Callback<void(const ResponseProto&)>& result_callback,
    const std::string& serialized_response) {
  ResponseProto response;
  if (!response.ParseFromString(serialized_response)) {
    OnApiCallFailed("Failed to parse response proto.");
    return;
  }
  result_callback.Run(response);
};

void CryptAuthClientImpl::OnApiCallFailed(const std::string& error_message) {
  error_callback_.Run(error_message);
}

// CryptAuthClientFactoryImpl
CryptAuthClientFactoryImpl::CryptAuthClientFactoryImpl(
    OAuth2TokenService* token_service,
    const std::string& account_id,
    scoped_refptr<net::URLRequestContextGetter> url_request_context,
    const DeviceClassifier& device_classifier)
    : token_service_(token_service),
      account_id_(account_id),
      url_request_context_(url_request_context),
      device_classifier_(device_classifier) {
}

CryptAuthClientFactoryImpl::~CryptAuthClientFactoryImpl() {
}

std::unique_ptr<CryptAuthClient> CryptAuthClientFactoryImpl::CreateInstance() {
  return base::MakeUnique<CryptAuthClientImpl>(
      base::WrapUnique(new CryptAuthApiCallFlow()),
      base::WrapUnique(
          new CryptAuthAccessTokenFetcherImpl(token_service_, account_id_)),
      url_request_context_, device_classifier_);
}

}  // namespace cryptauth
