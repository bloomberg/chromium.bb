// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attestation_object.h"

#include <utility>

#include "components/cbor/cbor_values.h"
#include "components/cbor/cbor_writer.h"
#include "device/fido/attestation_statement.h"

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

void AttestationObject::EraseAttestationStatement() {
  attestation_statement_.reset(new NoneAttestationStatement);

#if !defined(NDEBUG)
  std::vector<uint8_t> auth_data = authenticator_data_.SerializeToByteArray();
  // See diagram at https://w3c.github.io/webauthn/#sctn-attestation
  constexpr size_t kAAGUIDOffset =
      32 /* RP ID hash */ + 1 /* flags */ + 4 /* signature counter */;
  constexpr size_t kAAGUIDSize = 16;
  DCHECK(auth_data.size() >= kAAGUIDOffset + kAAGUIDSize);
  DCHECK(std::all_of(&auth_data[kAAGUIDOffset],
                     &auth_data[kAAGUIDOffset + kAAGUIDSize],
                     [](uint8_t v) { return v == 0; }));
#endif
}

bool AttestationObject::IsAttestationCertificateInappropriatelyIdentifying() {
  return attestation_statement_
      ->IsAttestationCertificateInappropriatelyIdentifying();
}

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
