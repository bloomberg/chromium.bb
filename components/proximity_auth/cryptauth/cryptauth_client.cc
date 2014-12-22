// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "components/proximity_auth/cryptauth/cryptauth_access_token_fetcher.h"
#include "components/proximity_auth/cryptauth/cryptauth_api_call_flow.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/switches.h"
#include "net/url_request/url_request_context_getter.h"

namespace proximity_auth {

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
const char kSetupEnrollmentPath[] = "enrollment/setupenrollment";
const char kFinishEnrollmentPath[] = "enrollment/finishenrollment";

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

CryptAuthClient::CryptAuthClient(
    scoped_ptr<CryptAuthAccessTokenFetcher> access_token_fetcher,
    scoped_refptr<net::URLRequestContextGetter> url_request_context)
    : url_request_context_(url_request_context),
      access_token_fetcher_(access_token_fetcher.Pass()),
      weak_ptr_factory_(this) {
}

CryptAuthClient::~CryptAuthClient() {
}

void CryptAuthClient::GetMyDevices(
    const cryptauth::GetMyDevicesRequest& request,
    const GetMyDevicesCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kGetMyDevicesPath, request, callback, error_callback);
}

void CryptAuthClient::FindEligibleUnlockDevices(
    const cryptauth::FindEligibleUnlockDevicesRequest& request,
    const FindEligibleUnlockDevicesCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kFindEligibleUnlockDevicesPath, request, callback,
              error_callback);
}

void CryptAuthClient::SendDeviceSyncTickle(
    const cryptauth::SendDeviceSyncTickleRequest& request,
    const SendDeviceSyncTickleCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kSendDeviceSyncTicklePath, request, callback, error_callback);
}

void CryptAuthClient::ToggleEasyUnlock(
    const cryptauth::ToggleEasyUnlockRequest& request,
    const ToggleEasyUnlockCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kToggleEasyUnlockPath, request, callback, error_callback);
}

void CryptAuthClient::SetupEnrollment(
    const cryptauth::SetupEnrollmentRequest& request,
    const SetupEnrollmentCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kSetupEnrollmentPath, request, callback, error_callback);
}

void CryptAuthClient::FinishEnrollment(
    const cryptauth::FinishEnrollmentRequest& request,
    const FinishEnrollmentCallback& callback,
    const ErrorCallback& error_callback) {
  MakeApiCall(kFinishEnrollmentPath, request, callback, error_callback);
}

scoped_ptr<CryptAuthApiCallFlow> CryptAuthClient::CreateFlow(
    const GURL& request_url) {
  return make_scoped_ptr(new CryptAuthApiCallFlow(request_url));
}

template <class RequestProto, class ResponseProto>
void CryptAuthClient::MakeApiCall(
    const std::string& request_path,
    const RequestProto& request_proto,
    const base::Callback<void(const ResponseProto&)>& response_callback,
    const ErrorCallback& error_callback) {
  if (flow_) {
    error_callback.Run(
        "Client has been used for another request. Do not reuse.");
    return;
  }

  std::string serialized_request;
  if (!request_proto.SerializeToString(&serialized_request)) {
    error_callback.Run(std::string("Failed to serialize ") +
                       request_proto.GetTypeName() + " proto.");
    return;
  }

  request_path_ = request_path;
  error_callback_ = error_callback;
  access_token_fetcher_->FetchAccessToken(base::Bind(
      &CryptAuthClient::OnAccessTokenFetched<ResponseProto>,
      weak_ptr_factory_.GetWeakPtr(), serialized_request, response_callback));
}

template <class ResponseProto>
void CryptAuthClient::OnAccessTokenFetched(
    const std::string& serialized_request,
    const base::Callback<void(const ResponseProto&)>& response_callback,
    const std::string& access_token) {
  if (access_token.empty()) {
    OnApiCallFailed("Failed to get a valid access token.");
    return;
  }

  flow_ = CreateFlow(CreateRequestUrl(request_path_));
  flow_->Start(url_request_context_.get(), access_token, serialized_request,
               base::Bind(&CryptAuthClient::OnFlowSuccess<ResponseProto>,
                          weak_ptr_factory_.GetWeakPtr(), response_callback),
               base::Bind(&CryptAuthClient::OnApiCallFailed,
                          weak_ptr_factory_.GetWeakPtr()));
}

template <class ResponseProto>
void CryptAuthClient::OnFlowSuccess(
    const base::Callback<void(const ResponseProto&)>& result_callback,
    const std::string& serialized_response) {
  ResponseProto response;
  if (!response.ParseFromString(serialized_response)) {
    OnApiCallFailed("Failed to parse response proto.");
    return;
  }
  result_callback.Run(response);
};

void CryptAuthClient::OnApiCallFailed(const std::string& error_message) {
  error_callback_.Run(error_message);
}

}  // proximity_auth
