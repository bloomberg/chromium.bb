// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "extensions/common/cast/cast_cert_validator.h"
#include "net/cert/pem_tokenizer.h"

namespace {

namespace cast_crypto = ::extensions::core_api::cast_crypto;

}  // namespace

namespace networking_private_crypto {

bool VerifyCredentials(
    const std::string& certificate,
    const std::vector<std::string>& intermediate_certificates,
    const std::string& signature,
    const std::string& data,
    const std::string& connected_mac) {
  static const char kErrorPrefix[] = "Device verification failed. ";

  std::vector<std::string> headers;
  headers.push_back("CERTIFICATE");

  // Convert certificate from PEM to raw DER
  net::PEMTokenizer pem_tok(certificate, headers);
  if (!pem_tok.GetNext()) {
    LOG(ERROR) << kErrorPrefix << "Failed to parse device certificate.";
    return false;
  }
  std::string der_certificate = pem_tok.data();

  // Convert intermediate certificates from PEM to raw DER
  std::vector<std::string> der_intermediate_certificates;
  for (size_t idx = 0; idx < intermediate_certificates.size(); ++idx) {
    net::PEMTokenizer ica_pem_tok(intermediate_certificates[idx], headers);
    if (ica_pem_tok.GetNext()) {
      der_intermediate_certificates.push_back(ica_pem_tok.data());
    } else {
      LOG(WARNING) << "Failed to parse intermediate certificates.";
    }
  }

  // Verify device certificate
  scoped_ptr<cast_crypto::CertVerificationContext> verification_context;
  cast_crypto::VerificationResult verification_result =
      cast_crypto::VerifyDeviceCert(der_certificate,
                                    der_intermediate_certificates,
                                    &verification_context);

  if (verification_result.Failure()) {
    LOG(ERROR) << kErrorPrefix << verification_result.GetLogString();
    return false;
  }

  // Check that the device listed in the certificate is correct.
  // Something like evt_e161 001a11ffacdf
  std::string common_name = verification_context->GetCommonName();
  std::string translated_mac;
  base::RemoveChars(connected_mac, ":", &translated_mac);
  if (!EndsWith(common_name, translated_mac, false)) {
    LOG(ERROR) << kErrorPrefix << "MAC addresses don't match.";
    return false;
  }

  // Use the public key from verified certificate to verify |signature| over
  // |data|.
  verification_result =
      verification_context->VerifySignatureOverData(signature, data);

  if (verification_result.Failure()) {
    LOG(ERROR) << kErrorPrefix << verification_result.GetLogString();
    return false;
  }
  return true;
}

}  // namespace networking_private_crypto
