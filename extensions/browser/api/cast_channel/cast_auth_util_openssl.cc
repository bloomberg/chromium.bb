// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "extensions/browser/api/cast_channel/cast_auth_ica.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_openssl.h"

namespace extensions {
namespace core_api {
namespace cast_channel {
namespace {

typedef crypto::ScopedOpenSSL<X509, X509_free>::Type ScopedX509;

}  // namespace

// This function does the following
// * Verifies that the trusted CA |response.intermediate_certificate| is
//   whitelisted for use.
// * Verifies that |response.client_auth_certificate| is signed
//   by the trusted CA certificate.
// * Verifies that |response.signature| matches the signature
//   of |peer_cert| by |response.client_auth_certificate|'s public
//   key.
//
// TODO(kmarshall): Report fine-grained errors from OpenSSL.
AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& peer_cert) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Get the public key of the ICA that was used to sign the client's cert.
  base::StringPiece ca_public_key_bytes;
  if (response.intermediate_certificate().size() <= 0) {
    ca_public_key_bytes = GetDefaultTrustedICAPublicKey();
  } else {
    ca_public_key_bytes =
        GetTrustedICAPublicKey(response.intermediate_certificate(0));
    if (ca_public_key_bytes.empty()) {
      LOG(ERROR) << "Couldn't find trusted ICA.";
      return AuthResult::CreateWithParseError(
          "failed to verify credentials: cert not signed by trusted CA",
          AuthResult::ERROR_FINGERPRINT_NOT_FOUND);
    }
  }

  // Parse the CA public key.
  const uint8_t* ca_ptr =
      reinterpret_cast<const uint8_t*>(ca_public_key_bytes.data());
  const uint8_t* ca_public_key_end = ca_ptr + ca_public_key_bytes.size();
  crypto::ScopedRSA ca_public_key_rsa(
      d2i_RSAPublicKey(NULL, &ca_ptr, ca_public_key_bytes.size()));
  if (!ca_public_key_rsa || ca_ptr != ca_public_key_end) {
    LOG(ERROR) << "Failed to import trusted public key.";
    return AuthResult::CreateWithParseError(
        "failed to import trusted public key.",
        AuthResult::ERROR_CERT_PARSING_FAILED);
  }
  crypto::ScopedEVP_PKEY ca_public_key(EVP_PKEY_new());
  if (!ca_public_key ||
      !EVP_PKEY_set1_RSA(ca_public_key.get(), ca_public_key_rsa.get())) {
    LOG(ERROR) << "Failed to initialize EVP_PKEY";
    return AuthResult::CreateWithParseError(
        "failed to initialize EVP_PKEY.",
        AuthResult::ERROR_CANNOT_EXTRACT_PUBLIC_KEY);
  }

  // Parse the client auth certificate.
  const uint8_t* client_cert_ptr = reinterpret_cast<const uint8_t*>(
      response.client_auth_certificate().data());
  const uint8_t* client_cert_end =
      client_cert_ptr +
      response.client_auth_certificate().size();
  const ScopedX509 client_cert(
      d2i_X509(NULL, &client_cert_ptr,
               response.client_auth_certificate().size()));
  if (!client_cert || client_cert_ptr != client_cert_end) {
    LOG(ERROR) << "Failed to parse certificate.";
    return AuthResult::CreateWithParseError(
        "failed to parse client_auth_certificate.",
        AuthResult::ERROR_CERT_PARSING_FAILED);
  }

  // Verify that the client auth certificate was signed by a trusted CA.
  if (X509_verify(client_cert.get(), ca_public_key.get()) <= 0) {
    LOG(ERROR) << "Certificate is not issued by a trusted CA.";
      return AuthResult::CreateWithParseError(
          "cert not signed by trusted CA",
          AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
  }

  // Get the client auth certificate's public key.
  const crypto::ScopedEVP_PKEY client_public_key(
      X509_get_pubkey(client_cert.get()));
  const int client_public_key_type = EVP_PKEY_id(client_public_key.get());
  if (client_public_key_type != EVP_PKEY_RSA) {
    LOG(ERROR) << "Expected RSA key type for client certificate, got "
               << client_public_key_type << " instead.";
    return AuthResult::CreateWithParseError(
        "couldn't extract public_key from client cert.",
        AuthResult::ERROR_CANNOT_EXTRACT_PUBLIC_KEY);
  }

  // Check that the SSL peer certificate was signed using the client's public
  // key.
  const crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());
  if (!ctx ||
      !EVP_DigestVerifyInit(ctx.get(), NULL, EVP_sha1(), NULL,
                            client_public_key.get()) ||
      !EVP_DigestVerifyUpdate(ctx.get(), peer_cert.data(), peer_cert.size())) {
    return AuthResult::CreateWithParseError(
        "error initializing payload verification operation.",
        AuthResult::ERROR_UNEXPECTED_AUTH_LIBRARY_RESULT);
  }
  const std::string& signature = response.signature();
  if (EVP_DigestVerifyFinal(
          ctx.get(),
          reinterpret_cast<uint8_t*>(const_cast<char*>(signature.data())),
          signature.size()) <= 0) {
    return AuthResult::CreateWithParseError(
        "payload verification failed.",
        AuthResult::ERROR_SIGNED_BLOBS_MISMATCH);
  }

  return AuthResult();
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

