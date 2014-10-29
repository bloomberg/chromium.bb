// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <cert.h>
#include <cryptohi.h>
#include <pk11pub.h>
#include <seccomon.h>
#include <string>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "extensions/browser/api/cast_channel/cast_auth_ica.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"

namespace extensions {
namespace core_api {
namespace cast_channel {
namespace {
typedef scoped_ptr<
    CERTCertificate,
    crypto::NSSDestroyer<CERTCertificate, CERT_DestroyCertificate> >
        ScopedCERTCertificate;

// Authenticates the given credentials:
// 1. |signature| verification of |data| using |certificate|.
// 2. |certificate| is signed by a trusted CA.
AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& data) {
  const std::string kErrorPrefix("Failed to verify credentials: ");
  const std::string& certificate = response.client_auth_certificate();
  const std::string& signature = response.signature();

  // If the list of intermediates is empty then use kPublicKeyICA1 as
  // the trusted CA (legacy case).
  // Otherwise, use the first intermediate in the list as long as it
  // is in the allowed list of intermediates.
  int num_intermediates = response.intermediate_certificate_size();

  VLOG(1) << "Response has " << num_intermediates << " intermediates";

  base::StringPiece ica;
  if (num_intermediates <= 0) {
    ica = GetDefaultTrustedICAPublicKey();
  } else {
    ica = GetTrustedICAPublicKey(response.intermediate_certificate(0));
  }
  if (ica.empty()) {
    return AuthResult::CreateWithParseError(
        "Disallowed intermediate cert",
        AuthResult::ERROR_FINGERPRINT_NOT_FOUND);
  }

  SECItem trusted_ca_key_der;
  trusted_ca_key_der.type = SECItemType::siDERCertBuffer;
  trusted_ca_key_der.data =
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(ica.data()));
  trusted_ca_key_der.len = ica.size();

  crypto::EnsureNSSInit();
  SECItem der_cert;
  der_cert.type = siDERCertBuffer;
  // Make a copy of certificate string so it is safe to type cast.
  der_cert.data = reinterpret_cast<unsigned char*>(const_cast<char*>(
      certificate.data()));
  der_cert.len = certificate.length();

  // Parse into a certificate structure.
  ScopedCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &der_cert, NULL, PR_FALSE, PR_TRUE));
  if (!cert.get()) {
    return AuthResult::CreateWithNSSError(
        "Failed to parse certificate.",
        AuthResult::ERROR_NSS_CERT_PARSING_FAILED, PORT_GetError());
  }

  // Check that the certificate is signed by trusted CA.
  // NOTE: We const_cast trusted_ca_key_der since on some platforms
  // SECKEY_ImportDERPublicKey API takes in SECItem* and not const
  // SECItem*.
  crypto::ScopedSECKEYPublicKey ca_public_key(
      SECKEY_ImportDERPublicKey(&trusted_ca_key_der, CKK_RSA));
  SECStatus verified = CERT_VerifySignedDataWithPublicKey(
      &cert->signatureWrap, ca_public_key.get(), NULL);
  if (verified != SECSuccess) {
    return AuthResult::CreateWithNSSError(
        "Cert not signed by trusted CA",
        AuthResult::ERROR_NSS_CERT_NOT_SIGNED_BY_TRUSTED_CA, PORT_GetError());
  }

  VLOG(1) << "Cert signed by trusted CA";

  // Verify that the |signature| matches |data|.
  crypto::ScopedSECKEYPublicKey public_key(CERT_ExtractPublicKey(cert.get()));
  if (!public_key.get()) {
    return AuthResult::CreateWithNSSError(
        "Unable to extract public key from certificate",
        AuthResult::ERROR_NSS_CANNOT_EXTRACT_PUBLIC_KEY, PORT_GetError());
  }
  SECItem signature_item;
  signature_item.type = siBuffer;
  signature_item.data = reinterpret_cast<unsigned char*>(
      const_cast<char*>(signature.data()));
  signature_item.len = signature.length();
  verified = VFY_VerifyDataDirect(
      reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())),
      data.size(),
      public_key.get(),
      &signature_item,
      SEC_OID_PKCS1_RSA_ENCRYPTION,
      SEC_OID_SHA1, NULL, NULL);

  if (verified != SECSuccess) {
    return AuthResult::CreateWithNSSError(
        "Signed blobs did not match",
        AuthResult::ERROR_NSS_SIGNED_BLOBS_MISMATCH,
        PORT_GetError());
  }

  VLOG(1) << "Signature verification succeeded";

  return AuthResult();
}

}  // namespace

AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const std::string& peer_cert) {
  if (peer_cert.empty()) {
    AuthResult result = AuthResult::CreateWithParseError(
        "Peer cert was empty.", AuthResult::ERROR_PEER_CERT_EMPTY);
    VLOG(1) << result.error_message;
    return result;
  }

  VLOG(1) << "Challenge reply: " << CastMessageToString(challenge_reply);
  DeviceAuthMessage auth_message;
  AuthResult result = ParseAuthMessage(challenge_reply, &auth_message);
  if (!result.success()) {
    VLOG(1) << result.error_message;
    return result;
  }

  const AuthResponse& response = auth_message.response();
  result = VerifyCredentials(response, peer_cert);
  if (!result.success()) {
    VLOG(1) << result.error_message
            << ", NSS error code: " << result.nss_error_code;
    return result;
  }

  return AuthResult();
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions
