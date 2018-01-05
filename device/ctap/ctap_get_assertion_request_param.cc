// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/ctap/ctap_get_assertion_request_param.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/cbor_writer.h"
#include "device/ctap/ctap_request_command.h"

namespace device {

CTAPGetAssertionRequestParam::CTAPGetAssertionRequestParam(
    std::string rp_id,
    std::vector<uint8_t> client_data_hash)
    : rp_id_(std::move(rp_id)),
      client_data_hash_(std::move(client_data_hash)) {}

CTAPGetAssertionRequestParam::CTAPGetAssertionRequestParam(
    CTAPGetAssertionRequestParam&& that) = default;

CTAPGetAssertionRequestParam& CTAPGetAssertionRequestParam::operator=(
    CTAPGetAssertionRequestParam&& other) = default;

CTAPGetAssertionRequestParam::~CTAPGetAssertionRequestParam() = default;

base::Optional<std::vector<uint8_t>>
CTAPGetAssertionRequestParam::SerializeToCBOR() const {
  cbor::CBORValue::MapValue cbor_map;
  cbor_map[cbor::CBORValue(1)] = cbor::CBORValue(rp_id_);
  cbor_map[cbor::CBORValue(2)] = cbor::CBORValue(client_data_hash_);

  if (allow_list_) {
    cbor::CBORValue::ArrayValue allow_list_array;
    for (const auto& descriptor : *allow_list_) {
      allow_list_array.push_back(descriptor.ConvertToCBOR());
    }
    cbor_map[cbor::CBORValue(3)] = cbor::CBORValue(std::move(allow_list_array));
  }

  if (pin_auth_) {
    cbor_map[cbor::CBORValue(6)] = cbor::CBORValue(*pin_auth_);
  }

  if (pin_protocol_) {
    cbor_map[cbor::CBORValue(7)] = cbor::CBORValue(*pin_protocol_);
  }

  auto serialized_param =
      cbor::CBORWriter::Write(cbor::CBORValue(std::move(cbor_map)));
  if (!serialized_param) {
    return base::nullopt;
  }

  std::vector<uint8_t> cbor_request({base::strict_cast<uint8_t>(
      CTAPRequestCommand::kAuthenticatorGetAssertion)});
  cbor_request.insert(cbor_request.end(), serialized_param->begin(),
                      serialized_param->end());
  return cbor_request;
}

CTAPGetAssertionRequestParam& CTAPGetAssertionRequestParam::SetAllowList(
    std::vector<PublicKeyCredentialDescriptor> allow_list) {
  allow_list_ = std::move(allow_list);
  return *this;
}

CTAPGetAssertionRequestParam& CTAPGetAssertionRequestParam::SetPinAuth(
    std::vector<uint8_t> pin_auth) {
  pin_auth_ = std::move(pin_auth);
  return *this;
}

CTAPGetAssertionRequestParam& CTAPGetAssertionRequestParam::SetPinProtocol(
    uint8_t pin_protocol) {
  pin_protocol_ = pin_protocol;
  return *this;
}

}  // namespace device
