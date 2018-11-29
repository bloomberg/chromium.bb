// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_get_assertion_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

CtapGetAssertionRequest::CtapGetAssertionRequest(std::string rp_id,
                                                 std::string client_data_json)
    : rp_id_(std::move(rp_id)),
      client_data_json_(std::move(client_data_json)),
      client_data_hash_(
          fido_parsing_utils::CreateSHA256Hash(client_data_json_)) {}

CtapGetAssertionRequest::CtapGetAssertionRequest(
    const CtapGetAssertionRequest& that) = default;

CtapGetAssertionRequest::CtapGetAssertionRequest(
    CtapGetAssertionRequest&& that) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    const CtapGetAssertionRequest& other) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    CtapGetAssertionRequest&& other) = default;

CtapGetAssertionRequest::~CtapGetAssertionRequest() = default;

std::vector<uint8_t> CtapGetAssertionRequest::EncodeAsCBOR() const {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(rp_id_);
  cbor_map[cbor::Value(2)] = cbor::Value(client_data_hash_);

  if (allow_list_) {
    cbor::Value::ArrayValue allow_list_array;
    for (const auto& descriptor : *allow_list_) {
      allow_list_array.push_back(descriptor.ConvertToCBOR());
    }
    cbor_map[cbor::Value(3)] = cbor::Value(std::move(allow_list_array));
  }

  if (pin_auth_) {
    cbor_map[cbor::Value(6)] = cbor::Value(*pin_auth_);
  }

  if (pin_protocol_) {
    cbor_map[cbor::Value(7)] = cbor::Value(*pin_protocol_);
  }

  cbor::Value::MapValue option_map;

  // User presence is required by default.
  if (!user_presence_required_) {
    option_map[cbor::Value(kUserPresenceMapKey)] =
        cbor::Value(user_presence_required_);
  }

  // User verification is not required by default.
  if (user_verification_ == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(option_map));
  }

  auto serialized_param = cbor::Writer::Write(cbor::Value(std::move(cbor_map)));
  DCHECK(serialized_param);

  std::vector<uint8_t> cbor_request({base::strict_cast<uint8_t>(
      CtapRequestCommand::kAuthenticatorGetAssertion)});
  cbor_request.insert(cbor_request.end(), serialized_param->begin(),
                      serialized_param->end());
  return cbor_request;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetUserVerification(
    UserVerificationRequirement user_verification) {
  user_verification_ = user_verification;
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetUserPresenceRequired(
    bool user_presence_required) {
  user_presence_required_ = user_presence_required;
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetAllowList(
    std::vector<PublicKeyCredentialDescriptor> allow_list) {
  allow_list_ = std::move(allow_list);
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetPinAuth(
    std::vector<uint8_t> pin_auth) {
  pin_auth_ = std::move(pin_auth);
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetPinProtocol(
    uint8_t pin_protocol) {
  pin_protocol_ = pin_protocol;
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetCableExtension(
    std::vector<CableDiscoveryData> cable_extension) {
  cable_extension_ = std::move(cable_extension);
  return *this;
}

CtapGetAssertionRequest& CtapGetAssertionRequest::SetAppId(std::string app_id) {
  app_id_ = std::move(app_id);
  alternative_application_parameter_ =
      std::array<uint8_t, crypto::kSHA256Length>();
  crypto::SHA256HashString(*app_id_, alternative_application_parameter_->data(),
                           alternative_application_parameter_->size());
  return *this;
}

bool CtapGetAssertionRequest::CheckResponseRpIdHash(
    const std::array<uint8_t, kRpIdHashLength>& response_rp_id_hash) {
  return response_rp_id_hash == fido_parsing_utils::CreateSHA256Hash(rp_id_) ||
         (app_id_ &&
          response_rp_id_hash == *alternative_application_parameter());
}

}  // namespace device
