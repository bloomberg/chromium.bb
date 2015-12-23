// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "extensions/common/cast/cast_cert_validator.h"
#include "net/cert/x509_certificate.h"

namespace extensions {
namespace api {
namespace cast_channel {
namespace {

const char* const kParseErrorPrefix = "Failed to parse auth message: ";

const unsigned char kAudioOnlyPolicy[] =
    {0x06, 0x0A, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xD6, 0x79, 0x02, 0x05, 0x02};

// The maximum number of days a cert can live for.
const int kMaxSelfSignedCertLifetimeInDays = 4;

namespace cast_crypto = ::extensions::api::cast_crypto;

// Extracts an embedded DeviceAuthMessage payload from an auth challenge reply
// message.
AuthResult ParseAuthMessage(const CastMessage& challenge_reply,
                            DeviceAuthMessage* auth_message) {
  if (challenge_reply.payload_type() != CastMessage_PayloadType_BINARY) {
    return AuthResult::CreateWithParseError(
        "Wrong payload type in challenge reply",
        AuthResult::ERROR_WRONG_PAYLOAD_TYPE);
  }
  if (!challenge_reply.has_payload_binary()) {
    return AuthResult::CreateWithParseError(
        "Payload type is binary but payload_binary field not set",
        AuthResult::ERROR_NO_PAYLOAD);
  }
  if (!auth_message->ParseFromString(challenge_reply.payload_binary())) {
    return AuthResult::CreateWithParseError(
        "Cannot parse binary payload into DeviceAuthMessage",
        AuthResult::ERROR_PAYLOAD_PARSING_FAILED);
  }

  VLOG(1) << "Auth message: " << AuthMessageToString(*auth_message);

  if (auth_message->has_error()) {
    return AuthResult::CreateWithParseError(
        "Auth message error: " +
            base::IntToString(auth_message->error().error_type()),
        AuthResult::ERROR_MESSAGE_ERROR);
  }
  if (!auth_message->has_response()) {
    return AuthResult::CreateWithParseError(
        "Auth message has no response field", AuthResult::ERROR_NO_RESPONSE);
  }
  return AuthResult();
}

AuthResult TranslateVerificationResult(
    const cast_crypto::VerificationResult& result) {
  AuthResult translated;
  translated.error_message = result.error_message;
  switch (result.error_type) {
    case cast_crypto::VerificationResult::ERROR_NONE:
      translated.error_type = AuthResult::ERROR_NONE;
      break;
    case cast_crypto::VerificationResult::ERROR_CERT_INVALID:
      translated.error_type = AuthResult::ERROR_CERT_PARSING_FAILED;
      break;
    case cast_crypto::VerificationResult::ERROR_CERT_UNTRUSTED:
      translated.error_type = AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA;
      break;
    case cast_crypto::VerificationResult::ERROR_SIGNATURE_INVALID:
      translated.error_type = AuthResult::ERROR_SIGNED_BLOBS_MISMATCH;
      break;
    case cast_crypto::VerificationResult::ERROR_INTERNAL:
      translated.error_type = AuthResult::ERROR_UNEXPECTED_AUTH_LIBRARY_RESULT;
      break;
    default:
      translated.error_type = AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA;
  }
  return translated;
}

}  // namespace

AuthResult::AuthResult()
    : error_type(ERROR_NONE), channel_policies(POLICY_NONE) {}

AuthResult::AuthResult(const std::string& error_message, ErrorType error_type)
    : error_message(error_message), error_type(error_type) {}

AuthResult::~AuthResult() {
}

// static
AuthResult AuthResult::CreateWithParseError(const std::string& error_message,
                                            ErrorType error_type) {
  return AuthResult(kParseErrorPrefix + error_message, error_type);
}

AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const net::X509Certificate& peer_cert) {
  DeviceAuthMessage auth_message;
  AuthResult result = ParseAuthMessage(challenge_reply, &auth_message);
  if (!result.success()) {
    return result;
  }

  // Get the DER-encoded form of the certificate.
  std::string peer_cert_der;
  if (!net::X509Certificate::GetDEREncoded(peer_cert.os_cert_handle(),
                                           &peer_cert_der) ||
      peer_cert_der.empty()) {
    return AuthResult::CreateWithParseError(
        "Could not create DER-encoded peer cert.",
        AuthResult::ERROR_CERT_PARSING_FAILED);
  }

  // Ensure the peer cert is valid and doesn't have an excessive remaining
  // lifetime. Although it is not verified as an X.509 certificate, the entire
  // structure is signed by the AuthResponse, so the validity field from X.509
  // is repurposed as this signature's expiration.
  base::Time expiry = peer_cert.valid_expiry();
  base::Time lifetime_limit =
      base::Time::Now() +
      base::TimeDelta::FromDays(kMaxSelfSignedCertLifetimeInDays);
  if (peer_cert.valid_start().is_null() ||
      peer_cert.valid_start() > base::Time::Now()) {
    return AuthResult::CreateWithParseError(
        "Certificate's valid start date is in the future.",
        AuthResult::ERROR_VALID_START_DATE_IN_FUTURE);
  }
  if (expiry.is_null() || peer_cert.HasExpired()) {
    return AuthResult::CreateWithParseError("Certificate has expired.",
                                            AuthResult::ERROR_CERT_EXPIRED);
  }
  if (expiry > lifetime_limit) {
    return AuthResult::CreateWithParseError(
        "Peer cert lifetime is too long.",
        AuthResult::ERROR_VALIDITY_PERIOD_TOO_LONG);
  }

  const AuthResponse& response = auth_message.response();
  result = VerifyCredentials(response, peer_cert_der);
  if (!result.success()) {
    return result;
  }

  std::string audio_policy(
      reinterpret_cast<const char*>(kAudioOnlyPolicy),
      (arraysize(kAudioOnlyPolicy) / sizeof(unsigned char)));
  if (response.client_auth_certificate().find(audio_policy) !=
      std::string::npos) {
    result.channel_policies |= AuthResult::POLICY_AUDIO_ONLY;
  }

  return result;
}

// This function does the following
// * Verifies that the trusted CA |response.intermediate_certificate| is
//   whitelisted for use.
// * Verifies that |response.client_auth_certificate| is signed
//   by the trusted CA certificate.
// * Verifies that |response.signature| matches the signature
//   of |signature_input| by |response.client_auth_certificate|'s public
//   key.
AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& signature_input) {
  // Verify the certificate
  scoped_ptr<cast_crypto::CertVerificationContext> verification_context;
  cast_crypto::VerificationResult ret = cast_crypto::VerifyDeviceCert(
      response.client_auth_certificate(),
      std::vector<std::string>(response.intermediate_certificate().begin(),
                               response.intermediate_certificate().end()),
      &verification_context);

  if (ret.Success())
    ret = verification_context->VerifySignatureOverData(response.signature(),
                                                        signature_input);

  return TranslateVerificationResult(ret);
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
