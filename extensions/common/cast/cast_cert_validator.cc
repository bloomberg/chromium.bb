// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/cast/cast_cert_validator.h"

#include <openssl/digest.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "extensions/browser/api/cast_channel/cast_auth_ica.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_openssl.h"
#include "net/ssl/scoped_openssl_types.h"

namespace extensions {
namespace api {
namespace cast_crypto {
namespace {

class CertVerificationContextImpl : public CertVerificationContext {
 public:
  // Takes ownership of the passed-in x509 object
  explicit CertVerificationContextImpl(net::ScopedX509 x509)
      : x509_(x509.Pass()) {}

  VerificationResult VerifySignatureOverData(
      const base::StringPiece& signature,
      const base::StringPiece& data) const override {
    // Retrieve public key object.
    crypto::ScopedEVP_PKEY public_key(X509_get_pubkey(x509_.get()));
    if (!public_key) {
      return VerificationResult(
          "Failed to extract device certificate public key.",
          VerificationResult::ERROR_CERT_INVALID);
    }
    // Make sure the key is RSA.
    const int public_key_type = EVP_PKEY_id(public_key.get());
    if (public_key_type != EVP_PKEY_RSA) {
      return VerificationResult(
          std::string("Expected RSA key type for client certificate, got ") +
              base::IntToString(public_key_type) + " instead.",
          VerificationResult::ERROR_CERT_INVALID);
    }
    // Verify signature.
    const crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());
    if (!ctx ||
        !EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha1(), nullptr,
                              public_key.get()) ||
        !EVP_DigestVerifyUpdate(ctx.get(), data.data(), data.size()) ||
        !EVP_DigestVerifyFinal(
            ctx.get(), reinterpret_cast<const uint8_t*>(signature.data()),
            signature.size())) {
      return VerificationResult("Signature verification failed.",
                                VerificationResult::ERROR_SIGNATURE_INVALID);
    }
    return VerificationResult();
  }

  std::string GetCommonName() const override {
    int common_name_length = X509_NAME_get_text_by_NID(
        x509_->cert_info->subject, NID_commonName, NULL, 0);
    if (common_name_length < 0)
      return std::string();
    std::string common_name;
    common_name_length = X509_NAME_get_text_by_NID(
        x509_->cert_info->subject, NID_commonName,
        base::WriteInto(&common_name,
                        static_cast<size_t>(common_name_length) + 1),
        common_name_length + 1);
    if (common_name_length < 0)
      return std::string();
    return common_name;
  }

 private:
  net::ScopedX509 x509_;
};

}  // namespace

VerificationResult::VerificationResult() : VerificationResult("", ERROR_NONE) {}

VerificationResult::VerificationResult(const std::string& in_error_message,
                                       ErrorType in_error_type)
    : error_type(in_error_type), error_message(in_error_message) {}

VerificationResult VerifyDeviceCert(
    const base::StringPiece& device_cert,
    const std::vector<std::string>& ica_certs,
    scoped_ptr<CertVerificationContext>* context) {
  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // If the list of intermediates is empty then use kPublicKeyICA1 as
  // the trusted CA (legacy case).
  // Otherwise, use the first intermediate in the list as long as it
  // is in the allowed list of intermediates.
  base::StringPiece ica_public_key_der =
      (ica_certs.size() == 0)
          ? cast_channel::GetDefaultTrustedICAPublicKey()
          : cast_channel::GetTrustedICAPublicKey(ica_certs[0]);

  if (ica_public_key_der.empty()) {
    return VerificationResult(
        "Device certificate is not signed by a trusted CA",
        VerificationResult::ERROR_CERT_UNTRUSTED);
  }

  // Initialize the ICA public key.
  crypto::ScopedRSA ica_public_key_rsa(RSA_public_key_from_bytes(
      reinterpret_cast<const uint8_t*>(ica_public_key_der.data()),
      ica_public_key_der.size()));
  if (!ica_public_key_rsa) {
    return VerificationResult("Failed to import trusted public key.",
                              VerificationResult::ERROR_INTERNAL);
  }
  crypto::ScopedEVP_PKEY ica_public_key_evp(EVP_PKEY_new());
  if (!ica_public_key_evp ||
      !EVP_PKEY_set1_RSA(ica_public_key_evp.get(), ica_public_key_rsa.get())) {
    return VerificationResult("Failed to import trusted public key.",
                              VerificationResult::ERROR_INTERNAL);
  }

  // Parse the device certificate.
  const uint8_t* device_cert_der_ptr =
      reinterpret_cast<const uint8_t*>(device_cert.data());
  const uint8_t* device_cert_der_end = device_cert_der_ptr + device_cert.size();
  net::ScopedX509 device_cert_x509(
      d2i_X509(NULL, &device_cert_der_ptr, device_cert.size()));
  if (!device_cert_x509 || device_cert_der_ptr != device_cert_der_end) {
    return VerificationResult("Failed to parse device certificate.",
                              VerificationResult::ERROR_CERT_INVALID);
  }

  // Verify device certificate.
  if (X509_verify(device_cert_x509.get(), ica_public_key_evp.get()) != 1) {
    return VerificationResult(
        "Device certificate signature verification failed.",
        VerificationResult::ERROR_CERT_INVALID);
  }

  if (context)
    context->reset(new CertVerificationContextImpl(device_cert_x509.Pass()));

  return VerificationResult();
}

bool SetTrustedCertificateAuthoritiesForTest(const std::string& keys,
                                             const std::string& signature) {
  return extensions::api::cast_channel::SetTrustedCertificateAuthorities(
      keys, signature);
}

}  // namespace cast_crypto
}  // namespace api
}  // namespace extensions
