// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/attestation_object.h"

#include <utility>

#include "components/cbor/cbor_values.h"
#include "components/cbor/cbor_writer.h"
#include "content/browser/webauth/attestation_statement.h"

namespace content {

namespace {
constexpr char kAuthDataKey[] = "authData";
constexpr char kFormatKey[] = "fmt";
constexpr char kAttestationKey[] = "attStmt";
}  // namespace

AttestationObject::AttestationObject(
    std::unique_ptr<AuthenticatorData> data,
    std::unique_ptr<AttestationStatement> statement)
    : authenticator_data_(std::move(data)),
      attestation_statement_(std::move(statement)) {}

std::vector<uint8_t> AttestationObject::SerializeToCBOREncodedBytes() {
  cbor::CBORValue::MapValue map;
  map[cbor::CBORValue(kFormatKey)] =
      cbor::CBORValue(attestation_statement_->format_name().c_str());
  map[cbor::CBORValue(kAuthDataKey)] =
      cbor::CBORValue(authenticator_data_->SerializeToByteArray());
  map[cbor::CBORValue(kAttestationKey)] =
      cbor::CBORValue(attestation_statement_->GetAsCBORMap());
  auto cbor = cbor::CBORWriter::Write(cbor::CBORValue(map));
  if (cbor.has_value()) {
    return cbor.value();
  }
  return std::vector<uint8_t>();
}

AttestationObject::~AttestationObject() {}

}  // namespace content
