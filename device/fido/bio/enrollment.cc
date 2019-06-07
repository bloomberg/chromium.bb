// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment.h"

#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetModality() {
  BioEnrollmentRequest request;
  request.get_modality = true;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetSensorInfo() {
  BioEnrollmentRequest request;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kGetFingerprintSensorInfo;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnrollBegin(
    const pin::TokenResponse& response) {
  BioEnrollmentRequest request;
  request.pin_protocol = 1;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kEnrollBegin;
  request.pin_auth = response.PinAuth(
      std::vector<uint8_t>{static_cast<uint8_t>(*request.modality),
                           static_cast<uint8_t>(*request.subcommand)});
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnrollNextSample(
    const pin::TokenResponse& response,
    std::vector<uint8_t> template_id) {
  BioEnrollmentRequest request;
  request.pin_protocol = 1;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kEnrollCaptureNextSample;
  request.params = cbor::Value::MapValue();
  request.params->emplace(
      static_cast<int>(BioEnrollmentSubCommandParam::kTemplateId),
      cbor::Value(template_id));

  std::vector<uint8_t> pin_auth =
      *cbor::Writer::Write(cbor::Value(*request.params));
  pin_auth.insert(pin_auth.begin(), static_cast<int>(*request.subcommand));
  pin_auth.insert(pin_auth.begin(), static_cast<int>(*request.modality));

  request.pin_auth = response.PinAuth(std::move(pin_auth));

  return request;
}

BioEnrollmentRequest::BioEnrollmentRequest(BioEnrollmentRequest&&) = default;
BioEnrollmentRequest& BioEnrollmentRequest::operator=(BioEnrollmentRequest&&) =
    default;
BioEnrollmentRequest::BioEnrollmentRequest() = default;
BioEnrollmentRequest::~BioEnrollmentRequest() = default;

template <typename T>
static base::Optional<T> ToBioEnrollmentEnum(uint8_t v) {
  // Check if enum-class is in range...
  if (v < static_cast<int>(T::kMin) || v > static_cast<int>(T::kMax)) {
    // ...to avoid UB.
    return base::nullopt;
  }
  return static_cast<T>(v);
}

// static
base::Optional<BioEnrollmentResponse> BioEnrollmentResponse::Parse(
    const base::Optional<cbor::Value>& cbor_response) {
  BioEnrollmentResponse response;

  if (!cbor_response || !cbor_response->is_map()) {
    return response;
  }

  const auto& response_map = cbor_response->GetMap();

  // modality
  auto it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kModality)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.modality =
        ToBioEnrollmentEnum<BioEnrollmentModality>(it->second.GetUnsigned());
    if (!response.modality) {
      return base::nullopt;
    }
  }

  // fingerprint kind
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kFingerprintKind)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.fingerprint_kind =
        ToBioEnrollmentEnum<BioEnrollmentFingerprintKind>(
            it->second.GetUnsigned());
    if (!response.fingerprint_kind) {
      return base::nullopt;
    }
  }

  // max captures required for enroll
  it = response_map.find(cbor::Value(static_cast<int>(
      BioEnrollmentResponseKey::kMaxCaptureSamplesRequiredForEnroll)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.max_samples_for_enroll = it->second.GetUnsigned();
  }

  // template id
  it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kTemplateId)));
  if (it != response_map.end()) {
    if (!it->second.is_bytestring()) {
      return base::nullopt;
    }
    response.template_id = it->second.GetBytestring();
  }

  // last enroll sample status
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.last_status = ToBioEnrollmentEnum<BioEnrollmentSampleStatus>(
        it->second.GetUnsigned());
    if (!response.last_status) {
      return base::nullopt;
    }
  }

  // remaining samples
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.remaining_samples = it->second.GetUnsigned();
  }

  return response;
}

BioEnrollmentResponse::BioEnrollmentResponse() = default;
BioEnrollmentResponse::BioEnrollmentResponse(BioEnrollmentResponse&&) = default;
BioEnrollmentResponse::~BioEnrollmentResponse() = default;

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const BioEnrollmentRequest& request) {
  cbor::Value::MapValue map;

  using Key = BioEnrollmentRequestKey;

  if (request.modality) {
    map.emplace(static_cast<int>(Key::kModality),
                static_cast<int>(*request.modality));
  }

  if (request.subcommand) {
    map.emplace(static_cast<int>(Key::kSubCommand),
                static_cast<int>(*request.subcommand));
  }

  if (request.params) {
    map.emplace(static_cast<int>(Key::kSubCommandParams), *request.params);
  }

  if (request.pin_protocol) {
    map.emplace(static_cast<int>(Key::kPinProtocol), *request.pin_protocol);
  }

  if (request.pin_auth) {
    map.emplace(static_cast<int>(Key::kPinAuth), *request.pin_auth);
  }

  if (request.get_modality) {
    map.emplace(static_cast<int>(Key::kGetModality), *request.get_modality);
  }

  return {CtapRequestCommand::kAuthenticatorBioEnrollmentPreview,
          cbor::Value(std::move(map))};
}

}  // namespace device
