// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/attestation_object.h"

#include <utility>

#include "components/cbor/cbor_values.h"
#include "components/cbor/cbor_writer.h"
#include "device/u2f/attestation_statement.h"

namespace device {

namespace {
constexpr char kAuthDataKey[] = "authData";
constexpr char kFormatKey[] = "fmt";
constexpr char kAttestationKey[] = "attStmt";
}  // namespace

AttestationObject::AttestationObject(
    AuthenticatorData data,
    std::unique_ptr<AttestationStatement> statement)
    : authenticator_data_(std::move(data)),
      attestation_statement_(std::move(statement)) {}

AttestationObject::AttestationObject(AttestationObject&& other) = default;
AttestationObject& AttestationObject::operator=(AttestationObject&& other) =
    default;

AttestationObject::~AttestationObject() = default;

std::vector<uint8_t> AttestationObject::SerializeToCBOREncodedBytes() const {
  cbor::CBORValue::MapValue map;
  map[cbor::CBORValue(kFormatKey)] =
      cbor::CBORValue(attestation_statement_->format_name());
  map[cbor::CBORValue(kAuthDataKey)] =
      cbor::CBORValue(authenticator_data_.SerializeToByteArray());
  map[cbor::CBORValue(kAttestationKey)] =
      cbor::CBORValue(attestation_statement_->GetAsCBORMap());
  return cbor::CBORWriter::Write(cbor::CBORValue(std::move(map)))
      .value_or(std::vector<uint8_t>());
}

}  // namespace device
