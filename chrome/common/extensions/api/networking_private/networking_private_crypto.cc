// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"

#include <openssl/digest.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_openssl_types.h"
#include "extensions/common/cast/cast_cert_validator.h"
#include "net/cert/pem_tokenizer.h"

namespace {

namespace cast_crypto = ::extensions::api::cast_crypto;

// Parses |pem_data| for a PEM block of |pem_type|.
// Returns true if a |pem_type| block is found, storing the decoded result in
// |der_output|.
bool GetDERFromPEM(const std::string& pem_data,
                   const std::string& pem_type,
                   std::vector<uint8_t>* der_output) {
  std::vector<std::string> headers;
  headers.push_back(pem_type);
  net::PEMTokenizer pem_tokenizer(pem_data, headers);
  if (!pem_tokenizer.GetNext()) {
    return false;
  }

  der_output->assign(pem_tokenizer.data().begin(), pem_tokenizer.data().end());
  return true;
}

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
  net::PEMTokenizer pem_tokenizer(certificate, headers);
  if (!pem_tokenizer.GetNext()) {
    LOG(ERROR) << kErrorPrefix << "Failed to parse device certificate.";
    return false;
  }
  std::string der_certificate = pem_tokenizer.data();

  // Convert intermediate certificates from PEM to raw DER
  std::vector<std::string> der_intermediate_certificates;
  for (size_t idx = 0; idx < intermediate_certificates.size(); ++idx) {
    net::PEMTokenizer ica_pem_tokenizer(intermediate_certificates[idx],
                                        headers);
    if (ica_pem_tokenizer.GetNext()) {
      der_intermediate_certificates.push_back(ica_pem_tokenizer.data());
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
    LOG(ERROR) << kErrorPrefix << verification_result.error_message;
    return false;
  }

  // Check that the device listed in the certificate is correct.
  // Something like evt_e161 001a11ffacdf
  std::string common_name = verification_context->GetCommonName();
  std::string translated_mac;
  base::RemoveChars(connected_mac, ":", &translated_mac);
  if (!base::EndsWith(common_name, translated_mac,
                      base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(ERROR) << kErrorPrefix << "MAC addresses don't match.";
    return false;
  }

  // Use the public key from verified certificate to verify |signature| over
  // |data|.
  verification_result =
      verification_context->VerifySignatureOverData(signature, data);

  if (verification_result.Failure()) {
    LOG(ERROR) << kErrorPrefix << verification_result.error_message;
    return false;
  }
  return true;
}

bool EncryptByteString(const std::vector<uint8_t>& pub_key_der,
                       const std::string& data,
                       std::vector<uint8_t>* encrypted_output) {
  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  crypto::ScopedRSA rsa(RSA_public_key_from_bytes(vector_as_array(&pub_key_der),
                                                  pub_key_der.size()));
  if (!rsa || RSA_size(rsa.get()) == 0) {
    LOG(ERROR) << "Failed to parse public key";
    return false;
  }

  encrypted_output->resize(RSA_size(rsa.get()));
  int encrypted_length = RSA_public_encrypt(
      data.size(), reinterpret_cast<const uint8_t*>(data.data()),
      vector_as_array(encrypted_output), rsa.get(), RSA_PKCS1_PADDING);
  if (encrypted_length < 0) {
    LOG(ERROR) << "Error during decryption";
    return false;
  }
  encrypted_output->resize(encrypted_length);
  return true;
}

bool DecryptByteString(const std::string& private_key_pem,
                       const std::vector<uint8_t>& encrypted_data,
                       std::string* decrypted_output) {
  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  std::vector<uint8_t> private_key_data;
  if (!GetDERFromPEM(private_key_pem, "PRIVATE KEY", &private_key_data)) {
    LOG(ERROR) << "Failed to parse private key PEM.";
    return false;
  }
  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(private_key_data));
  if (!private_key || !private_key->key()) {
    LOG(ERROR) << "Failed to parse private key DER.";
    return false;
  }

  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(private_key->key()));
  if (!rsa || RSA_size(rsa.get()) == 0) {
    LOG(ERROR) << "Failed to get RSA key.";
    return false;
  }

  uint8_t* output = reinterpret_cast<uint8_t*>(
      base::WriteInto(decrypted_output, RSA_size(rsa.get()) + 1));
  int output_length =
      RSA_private_decrypt(encrypted_data.size(), &encrypted_data[0], output,
                          rsa.get(), RSA_PKCS1_PADDING);
  if (output_length < 0) {
    LOG(ERROR) << "Error during decryption.";
    return false;
  }
  decrypted_output->resize(output_length);
  return true;
}

}  // namespace networking_private_crypto
