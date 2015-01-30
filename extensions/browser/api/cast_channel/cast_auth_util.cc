// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "extensions/common/cast/cast_cert_validator.h"

namespace extensions {
namespace core_api {
namespace cast_channel {
namespace {

const char* const kParseErrorPrefix = "Failed to parse auth message: ";

const unsigned char kAudioOnlyPolicy[] =
    {0x06, 0x0A, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xD6, 0x79, 0x02, 0x05, 0x02};

namespace cast_crypto = ::extensions::core_api::cast_crypto;

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
  translated.nss_error_code = result.library_error_code;
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
  };
  return translated;
}

}  // namespace

AuthResult::AuthResult()
    : error_type(ERROR_NONE), nss_error_code(0), channel_policies(POLICY_NONE) {
}

AuthResult::~AuthResult() {
}

// static
AuthResult AuthResult::CreateWithParseError(const std::string& error_message,
                                            ErrorType error_type) {
  return AuthResult(kParseErrorPrefix + error_message, error_type, 0);
}

// static
AuthResult AuthResult::CreateWithNSSError(const std::string& error_message,
                                          ErrorType error_type,
                                          int nss_error_code) {
  return AuthResult(error_message, error_type, nss_error_code);
}

AuthResult::AuthResult(const std::string& error_message,
                       ErrorType error_type,
                       int nss_error_code)
    : error_message(error_message),
      error_type(error_type),
      nss_error_code(nss_error_code) {
}

AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const std::string& peer_cert) {
  if (peer_cert.empty()) {
    AuthResult result = AuthResult::CreateWithParseError(
        "Peer cert was empty.", AuthResult::ERROR_PEER_CERT_EMPTY);
    return result;
  }

  DeviceAuthMessage auth_message;
  AuthResult result = ParseAuthMessage(challenge_reply, &auth_message);
  if (!result.success()) {
    return result;
  }

  const AuthResponse& response = auth_message.response();
  result = VerifyCredentials(response, peer_cert);
  if (!result.success()) {
    return result;
  }

  const std::string& audio_policy =
      std::string(reinterpret_cast<const char*>(kAudioOnlyPolicy),
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
//   of |peer_cert| by |response.client_auth_certificate|'s public
//   key.
AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& peer_cert) {
  // Verify the certificate
  scoped_ptr<cast_crypto::CertVerificationContext> verification_context;
  cast_crypto::VerificationResult ret = cast_crypto::VerifyDeviceCert(
      response.client_auth_certificate(),
      std::vector<std::string>(response.intermediate_certificate().begin(),
                               response.intermediate_certificate().end()),
      &verification_context);

  if (ret.Success())
    ret = verification_context->VerifySignatureOverData(response.signature(),
                                                        peer_cert);

  return TranslateVerificationResult(ret);
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions
