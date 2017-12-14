// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/fido_attestation_statement.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "content/browser/webauth/authenticator_utils.h"
#include "content/browser/webauth/cbor/cbor_writer.h"

namespace content {

namespace {
constexpr char kFidoFormatName[] = "fido-u2f";
constexpr char kSignatureKey[] = "sig";
constexpr char kX509CertKey[] = "x5c";
}  // namespace

// static
std::unique_ptr<FidoAttestationStatement>
FidoAttestationStatement::CreateFromU2fRegisterResponse(
    const std::vector<uint8_t>& u2f_data) {
  std::vector<std::vector<uint8_t>> x509_certificates;
  std::vector<uint8_t> x509_cert;

  // Extract the length length of the credential (i.e. of U2FResponse key
  // handle). Length is big endian and is located at position 66 in the data.
  // Note that U2F responses only use one byte for length.
  std::vector<uint8_t> id_length(2, 0);
  CHECK_GE(u2f_data.size(), authenticator_utils::kU2fResponseLengthPos + 1);
  id_length[1] = u2f_data[authenticator_utils::kU2fResponseLengthPos];
  size_t id_end_byte = authenticator_utils::kU2fResponseKeyHandleStartPos +
                       ((base::strict_cast<uint32_t>(id_length[0]) << 8) |
                        (base::strict_cast<uint32_t>(id_length[1])));

  // Parse x509 cert to get cert length (which is variable).
  // TODO: Support responses with multiple certs.
  int num_bytes = 0;
  uint32_t cert_length = 0;
  // The first x509 byte is a tag, so we skip it.
  size_t first_length_byte =
      base::strict_cast<size_t>(u2f_data[id_end_byte + 1]);

  // If the first length byte is less than 127, it is the length. If it is
  // greater than 128, it indicates the number of following bytes that encode
  // the length.
  if (first_length_byte > 127) {
    num_bytes = first_length_byte - 128;

    // x509 cert length, interpreted big-endian.
    for (int i = 1; i <= num_bytes; i++) {
      // Account for tag byte and length details byte.
      cert_length |= base::checked_cast<uint32_t>(u2f_data[id_end_byte + 1 + i]
                                                  << ((num_bytes - i) * 8));
    }
  } else {
    cert_length = first_length_byte;
  }

  size_t x509_length = 1 /* tag byte */ + 1 /* first length byte */
                       + num_bytes /* # bytes of length */ + cert_length;

  authenticator_utils::Append(
      &x509_cert,
      authenticator_utils::Extract(u2f_data, id_end_byte, x509_length));
  x509_certificates.push_back(x509_cert);

  // The remaining bytes are the signature.
  std::vector<uint8_t> signature;
  size_t signature_start_byte = id_end_byte + x509_length;
  authenticator_utils::Append(
      &signature,
      authenticator_utils::Extract(u2f_data, signature_start_byte,
                                   u2f_data.size() - signature_start_byte));
  return std::make_unique<FidoAttestationStatement>(signature,
                                                    x509_certificates);
}

FidoAttestationStatement::FidoAttestationStatement(
    std::vector<uint8_t> signature,
    std::vector<std::vector<uint8_t>> x509_certificates)
    : AttestationStatement(kFidoFormatName),
      signature_(std::move(signature)),
      x509_certificates_(std::move(x509_certificates)) {}

CBORValue::MapValue FidoAttestationStatement::GetAsCBORMap() {
  CBORValue::MapValue attstmt_map;
  attstmt_map[CBORValue(kSignatureKey)] = CBORValue(signature_);

  std::vector<CBORValue> array;
  for (auto cert : x509_certificates_) {
    array.push_back(CBORValue(cert));
  }
  attstmt_map[CBORValue(kX509CertKey)] = CBORValue(array);
  return attstmt_map;
}

FidoAttestationStatement::~FidoAttestationStatement() {}

}  // namespace content
