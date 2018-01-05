// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/service_manager_connection.h"
#include "crypto/sha2.h"
#include "device/u2f/u2f_hid_discovery.h"
#include "device/u2f/u2f_register.h"
#include "device/u2f/u2f_request.h"
#include "device/u2f/u2f_return_code.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {
constexpr int32_t kCoseEs256 = -7;

bool HasValidAlgorithm(
    const std::vector<webauth::mojom::PublicKeyCredentialParametersPtr>&
        parameters) {
  for (const auto& params : parameters) {
    if (params->algorithm_identifier == kCoseEs256)
      return true;
  }
  return false;
}

webauth::mojom::PublicKeyCredentialInfoPtr CreatePublicKeyCredentialInfo(
    CollectedClientData client_data,
    device::RegisterResponseData response_data) {
  auto credential_info = webauth::mojom::PublicKeyCredentialInfo::New();
  std::string client_data_json = client_data.SerializeToJson();
  credential_info->client_data_json.assign(client_data_json.begin(),
                                           client_data_json.end());
  credential_info->raw_id = response_data.raw_id();
  credential_info->id = response_data.GetId();
  auto response = webauth::mojom::AuthenticatorResponse::New();
  response->attestation_object =
      response_data.GetCBOREncodedAttestationObject();
  credential_info->response = std::move(response);
  return credential_info;
}
}  // namespace

AuthenticatorImpl::AuthenticatorImpl(RenderFrameHost* render_frame_host)
    : timer_(std::make_unique<base::OneShotTimer>()),
      render_frame_host_(render_frame_host),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
}

AuthenticatorImpl::AuthenticatorImpl(RenderFrameHost* render_frame_host,
                                     service_manager::Connector* connector,
                                     std::unique_ptr<base::OneShotTimer> timer)
    : timer_(std::move(timer)),
      render_frame_host_(render_frame_host),
      connector_(connector),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
}

AuthenticatorImpl::~AuthenticatorImpl() {}

void AuthenticatorImpl::Bind(webauth::mojom::AuthenticatorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

// mojom::Authenticator
void AuthenticatorImpl::MakeCredential(
    webauth::mojom::MakePublicKeyCredentialOptionsPtr options,
    MakeCredentialCallback callback) {
  // Ensure no other operations are in flight.
  if (u2f_request_) {
    std::move(callback).Run(
        webauth::mojom::AuthenticatorStatus::PENDING_REQUEST, nullptr);
    return;
  }

  // Steps 6 & 7 of https://w3c.github.io/webauthn/#createCredential
  // opaque origin
  url::Origin caller_origin = render_frame_host_->GetLastCommittedOrigin();
  if (caller_origin.unique()) {
    std::move(callback).Run(
        webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
    return;
  }

  std::string relying_party_id;
  if (options->relying_party->id) {
    // TODO(kpaulhamus): Check if relyingPartyId is a registrable domain
    // suffix of and equal to effectiveDomain and set relyingPartyId
    // appropriately.
    // TODO(kpaulhamus): Add unit tests for domains. http://crbug.com/785950.
    relying_party_id = *options->relying_party->id;
  } else {
    // Use the effective domain of the caller origin.
    relying_party_id = caller_origin.host();
    DCHECK(!relying_party_id.empty());
  }

  // Check that at least one of the cryptographic parameters is supported.
  // Only ES256 is currently supported by U2F_V2.
  if (!HasValidAlgorithm(options->public_key_parameters)) {
    std::move(callback).Run(
        webauth::mojom::AuthenticatorStatus::NOT_SUPPORTED_ERROR, nullptr);
    return;
  }

  DCHECK(make_credential_response_callback_.is_null());
  make_credential_response_callback_ = std::move(callback);
  client_data_ = CollectedClientData::Create(client_data::kCreateType,
                                             caller_origin.Serialize(),
                                             std::move(options->challenge));

  // SHA-256 hash of the JSON data structure
  std::vector<uint8_t> client_data_hash(crypto::kSHA256Length);
  crypto::SHA256HashString(client_data_.SerializeToJson(),
                           client_data_hash.data(), client_data_hash.size());

  // The application parameter is the SHA-256 hash of the UTF-8 encoding of
  // the application identity (i.e. relying_party_id) of the application
  // requesting the registration.
  std::vector<uint8_t> application_parameter(crypto::kSHA256Length);
  crypto::SHA256HashString(relying_party_id, application_parameter.data(),
                           application_parameter.size());

  // Start the timer (step 16 - https://w3c.github.io/webauthn/#makeCredential).
  DCHECK(timer_);
  timer_->Start(
      FROM_HERE, options->adjusted_timeout,
      base::Bind(&AuthenticatorImpl::OnTimeout, base::Unretained(this)));

  if (!connector_) {
    connector_ = ServiceManagerConnection::GetForProcess()->GetConnector();
  }

  DCHECK(!u2f_discovery_);
  u2f_discovery_ = std::make_unique<device::U2fHidDiscovery>(connector_);

  // Per fido-u2f-raw-message-formats:
  // The challenge parameter is the SHA-256 hash of the Client Data,
  // Among other things, the Client Data contains the challenge from the
  // relying party (hence the name of the parameter).
  device::U2fRegister::RegisterResponseCallback response_callback = base::Bind(
      &AuthenticatorImpl::OnRegisterResponse, weak_factory_.GetWeakPtr());

  // Extract list of credentials to exclude.
  std::vector<std::vector<uint8_t>> registered_keys;
  for (const auto& credential : options->exclude_credentials) {
    registered_keys.push_back(credential->id);
  }

  // TODO(kpaulhamus): Mock U2fRegister for unit tests.
  // http://crbug.com/785955.
  u2f_request_ = device::U2fRegister::TryRegistration(
      registered_keys, client_data_hash, application_parameter,
      relying_party_id, {u2f_discovery_.get()}, std::move(response_callback));
}

// Callback to handle the async registration response from a U2fDevice.
void AuthenticatorImpl::OnRegisterResponse(
    device::U2fReturnCode status_code,
    base::Optional<device::RegisterResponseData> response_data) {
  timer_->Stop();

  switch (status_code) {
    case device::U2fReturnCode::CONDITIONS_NOT_SATISFIED:
      // Duplicate registration.
      std::move(make_credential_response_callback_)
          .Run(webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
      break;
    case device::U2fReturnCode::FAILURE:
    case device::U2fReturnCode::INVALID_PARAMS:
      std::move(make_credential_response_callback_)
          .Run(webauth::mojom::AuthenticatorStatus::UNKNOWN_ERROR, nullptr);
      break;
    case device::U2fReturnCode::SUCCESS:
      DCHECK(response_data.has_value());
      std::move(make_credential_response_callback_)
          .Run(webauth::mojom::AuthenticatorStatus::SUCCESS,
               CreatePublicKeyCredentialInfo(std::move(client_data_),
                                             std::move(*response_data)));
      break;
  }
  Cleanup();
}

void AuthenticatorImpl::OnTimeout() {
  DCHECK(make_credential_response_callback_);
  Cleanup();
  std::move(make_credential_response_callback_)
      .Run(webauth::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
}

void AuthenticatorImpl::Cleanup() {
  u2f_request_.reset();
  u2f_discovery_.reset();
  client_data_ = CollectedClientData();
}
}  // namespace content
