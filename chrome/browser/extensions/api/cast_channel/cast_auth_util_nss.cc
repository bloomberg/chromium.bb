// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_auth_util.h"

#include <cert.h>
#include <cryptohi.h>
#include <pk11pub.h>
#include <seccomon.h>
#include <string>

#include "base/logging.h"
#include "chrome/browser/extensions/api/cast_channel/cast_channel.pb.h"
#include "chrome/browser/extensions/api/cast_channel/cast_message_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"

namespace {

// Public key of the certificate with which the peer cert should be signed.
static const unsigned char kCAPublicKeyDER[] = {
    0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xbc, 0x22, 0x80,
    0xbd, 0x80, 0xf6, 0x3a, 0x21, 0x00, 0x3b, 0xae, 0x76, 0x5e, 0x35, 0x7f,
    0x3d, 0xc3, 0x64, 0x5c, 0x55, 0x94, 0x86, 0x34, 0x2f, 0x05, 0x87, 0x28,
    0xcd, 0xf7, 0x69, 0x8c, 0x17, 0xb3, 0x50, 0xa7, 0xb8, 0x82, 0xfa, 0xdf,
    0xc7, 0x43, 0x2d, 0xd6, 0x7e, 0xab, 0xa0, 0x6f, 0xb7, 0x13, 0x72, 0x80,
    0xa4, 0x47, 0x15, 0xc1, 0x20, 0x99, 0x50, 0xcd, 0xec, 0x14, 0x62, 0x09,
    0x5b, 0xa4, 0x98, 0xcd, 0xd2, 0x41, 0xb6, 0x36, 0x4e, 0xff, 0xe8, 0x2e,
    0x32, 0x30, 0x4a, 0x81, 0xa8, 0x42, 0xa3, 0x6c, 0x9b, 0x33, 0x6e, 0xca,
    0xb2, 0xf5, 0x53, 0x66, 0xe0, 0x27, 0x53, 0x86, 0x1a, 0x85, 0x1e, 0xa7,
    0x39, 0x3f, 0x4a, 0x77, 0x8e, 0xfb, 0x54, 0x66, 0x66, 0xfb, 0x58, 0x54,
    0xc0, 0x5e, 0x39, 0xc7, 0xf5, 0x50, 0x06, 0x0b, 0xe0, 0x8a, 0xd4, 0xce,
    0xe1, 0x6a, 0x55, 0x1f, 0x8b, 0x17, 0x00, 0xe6, 0x69, 0xa3, 0x27, 0xe6,
    0x08, 0x25, 0x69, 0x3c, 0x12, 0x9d, 0x8d, 0x05, 0x2c, 0xd6, 0x2e, 0xa2,
    0x31, 0xde, 0xb4, 0x52, 0x50, 0xd6, 0x20, 0x49, 0xde, 0x71, 0xa0, 0xf9,
    0xad, 0x20, 0x40, 0x12, 0xf1, 0xdd, 0x25, 0xeb, 0xd5, 0xe6, 0xb8, 0x36,
    0xf4, 0xd6, 0x8f, 0x7f, 0xca, 0x43, 0xdc, 0xd7, 0x10, 0x5b, 0xe6, 0x3f,
    0x51, 0x8a, 0x85, 0xb3, 0xf3, 0xff, 0xf6, 0x03, 0x2d, 0xcb, 0x23, 0x4f,
    0x9c, 0xad, 0x18, 0xe7, 0x93, 0x05, 0x8c, 0xac, 0x52, 0x9a, 0xf7, 0x4c,
    0xe9, 0x99, 0x7a, 0xbe, 0x6e, 0x7e, 0x4d, 0x0a, 0xe3, 0xc6, 0x1c, 0xa9,
    0x93, 0xfa, 0x3a, 0xa5, 0x91, 0x5d, 0x1c, 0xbd, 0x66, 0xeb, 0xcc, 0x60,
    0xdc, 0x86, 0x74, 0xca, 0xcf, 0xf8, 0x92, 0x1c, 0x98, 0x7d, 0x57, 0xfa,
    0x61, 0x47, 0x9e, 0xab, 0x80, 0xb7, 0xe4, 0x48, 0x80, 0x2a, 0x92, 0xc5,
    0x1b, 0x02, 0x03, 0x01, 0x00, 0x01 };

typedef scoped_ptr_malloc<
    CERTCertificate,
    crypto::NSSDestroyer<CERTCertificate, CERT_DestroyCertificate> >
ScopedCERTCertificate;

// Parses out DeviceAuthMessage from CastMessage
static bool ParseAuthMessage(
    const extensions::api::cast_channel::CastMessage& challenge_reply,
    extensions::api::cast_channel::DeviceAuthMessage* auth_message) {
  if (challenge_reply.payload_type() !=
      extensions::api::cast_channel::CastMessage_PayloadType_BINARY) {
    DVLOG(1) << "Wrong payload type in challenge reply";
    return false;
  }
  if (!challenge_reply.has_payload_binary()) {
    DVLOG(1) << "Payload type is binary but payload_binary field not set";
    return false;
  }
  if (!auth_message->ParseFromString(challenge_reply.payload_binary())) {
    DVLOG(1) << "Cannot parse binary payload into DeviceAuthMessage";
    return false;
  }
  DVLOG(1) << "Auth message: " << AuthMessageToString(*auth_message);
  if (auth_message->has_error()) {
    DVLOG(1) << "Auth message error: " << auth_message->error().error_type();
    return false;
  }
  if (!auth_message->has_response()) {
    DVLOG(1) << "Auth message has no response field";
    return false;
  }
  return true;
}

// Authenticates the given credentials:
// 1. |signature| verification of |data| using |certificate|.
// 2. |certificate| is signed by a trusted CA.
bool VerifyCredentials(const std::string& certificate,
                       const std::string& signature,
                       const std::string& data) {
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
     DVLOG(1) << "Failed to parse certificate.";
     return false;
  }

  // Check that the certificate is signed by trusted CA.
  SECItem trusted_ca_key_der_item;
  trusted_ca_key_der_item.type = siDERCertBuffer;
  trusted_ca_key_der_item.data = const_cast<unsigned char*>(kCAPublicKeyDER);
  trusted_ca_key_der_item.len = sizeof(kCAPublicKeyDER);
  crypto::ScopedSECKEYPublicKey ca_public_key(
      SECKEY_ImportDERPublicKey(&trusted_ca_key_der_item, CKK_RSA));
  SECStatus verified = CERT_VerifySignedDataWithPublicKey(
      &cert->signatureWrap, ca_public_key.get(), NULL);
  if (verified != SECSuccess) {
    DVLOG(1)<< "Cert not signed by trusted CA";
    return false;
  }

  // Verify that the |signature| matches |data|.
  crypto::ScopedSECKEYPublicKey public_key(CERT_ExtractPublicKey(cert.get()));
  if (!public_key.get()) {
    DVLOG(1) << "Unable to extract public key from certificate.";
    return false;
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
    DVLOG(1) << "Signed blobs did not match.";
    return false;
  }
  return true;
}

}  // namespace

namespace extensions {
namespace api {
namespace cast_channel {

bool AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                const std::string& peer_cert) {
  if (peer_cert.empty())
    return false;

  DVLOG(1) << "Challenge reply: " << CastMessageToString(challenge_reply);
  DeviceAuthMessage auth_message;
  if (!ParseAuthMessage(challenge_reply, &auth_message))
    return false;

  const AuthResponse& response = auth_message.response();
  return VerifyCredentials(response.client_auth_certificate(),
                           response.signature(),
                           peer_cert);
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
