// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/fido_attestation_statement.h"

#include <utility>

#include "base/logging.h"
#include "device/u2f/u2f_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace device {

namespace {
constexpr char kFidoFormatName[] = "fido-u2f";
constexpr char kSignatureKey[] = "sig";
constexpr char kX509CertKey[] = "x5c";
}  // namespace

// static
std::unique_ptr<FidoAttestationStatement>
FidoAttestationStatement::CreateFromU2fRegisterResponse(
    base::span<const uint8_t> u2f_data) {
  CBS response, cert;
  CBS_init(&response, u2f_data.data(), u2f_data.size());

  // The format of |u2f_data| is specified here:
  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-response-message-success
  uint8_t credential_length;
  if (!CBS_skip(&response, u2f_parsing_utils::kU2fResponseKeyHandleLengthPos) ||
      !CBS_get_u8(&response, &credential_length) ||
      !CBS_skip(&response, credential_length) ||
      !CBS_get_asn1_element(&response, &cert, CBS_ASN1_SEQUENCE)) {
    DLOG(ERROR)
        << "Invalid U2F response. Unable to unpack attestation statement.";
    return nullptr;
  }

  std::vector<std::vector<uint8_t>> x509_certificates;
  x509_certificates.emplace_back(CBS_data(&cert),
                                 CBS_data(&cert) + CBS_len(&cert));

  // The remaining bytes are the signature.
  std::vector<uint8_t> signature(CBS_data(&response),
                                 CBS_data(&response) + CBS_len(&response));
  return std::make_unique<FidoAttestationStatement>(
      std::move(signature), std::move(x509_certificates));
}

FidoAttestationStatement::FidoAttestationStatement(
    std::vector<uint8_t> signature,
    std::vector<std::vector<uint8_t>> x509_certificates)
    : AttestationStatement(kFidoFormatName),
      signature_(std::move(signature)),
      x509_certificates_(std::move(x509_certificates)) {}

FidoAttestationStatement::~FidoAttestationStatement() = default;

cbor::CBORValue::MapValue FidoAttestationStatement::GetAsCBORMap() {
  cbor::CBORValue::MapValue attestation_statement_map;
  attestation_statement_map[cbor::CBORValue(kSignatureKey)] =
      cbor::CBORValue(signature_);

  std::vector<cbor::CBORValue> certificate_array;
  for (const auto& cert : x509_certificates_) {
    certificate_array.push_back(cbor::CBORValue(cert));
  }

  attestation_statement_map[cbor::CBORValue(kX509CertKey)] =
      cbor::CBORValue(std::move(certificate_array));

  return attestation_statement_map;
}

}  // namespace device
