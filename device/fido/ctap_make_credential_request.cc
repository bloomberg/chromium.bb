// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_make_credential_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    std::string in_client_data_json,
    PublicKeyCredentialRpEntity in_rp,
    PublicKeyCredentialUserEntity in_user,
    PublicKeyCredentialParams in_public_key_credential_params)
    : client_data_json(std::move(in_client_data_json)),
      client_data_hash(fido_parsing_utils::CreateSHA256Hash(client_data_json)),
      rp(std::move(in_rp)),
      user(std::move(in_user)),
      public_key_credential_params(std::move(in_public_key_credential_params)) {
}

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest::~CtapMakeCredentialRequest() = default;

// static
std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
CtapMakeCredentialRequest::EncodeAsCBOR(
    const CtapMakeCredentialRequest& request) {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(request.client_data_hash);
  cbor_map[cbor::Value(2)] = request.rp.ConvertToCBOR();
  cbor_map[cbor::Value(3)] =
      PublicKeyCredentialUserEntity::ConvertToCBOR(request.user);
  cbor_map[cbor::Value(4)] =
      request.public_key_credential_params.ConvertToCBOR();
  if (request.exclude_list) {
    cbor::Value::ArrayValue exclude_list_array;
    for (const auto& descriptor : *request.exclude_list) {
      exclude_list_array.push_back(descriptor.ConvertToCBOR());
    }
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(exclude_list_array));
  }

  if (request.hmac_secret) {
    cbor::Value::MapValue extensions;
    extensions[cbor::Value(kExtensionHmacSecret)] = cbor::Value(true);
    cbor_map[cbor::Value(6)] = cbor::Value(std::move(extensions));
  }

  if (request.pin_auth) {
    cbor_map[cbor::Value(8)] = cbor::Value(*request.pin_auth);
  }

  if (request.pin_protocol) {
    cbor_map[cbor::Value(9)] = cbor::Value(*request.pin_protocol);
  }

  cbor::Value::MapValue option_map;

  // Resident keys are not required by default.
  if (request.resident_key_required) {
    option_map[cbor::Value(kResidentKeyMapKey)] =
        cbor::Value(request.resident_key_required);
  }

  // User verification is not required by default.
  if (request.user_verification == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(7)] = cbor::Value(std::move(option_map));
  }

  return std::make_pair(CtapRequestCommand::kAuthenticatorMakeCredential,
                        cbor::Value(std::move(cbor_map)));
}

}  // namespace device
