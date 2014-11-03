// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"

namespace extensions {
namespace core_api {
namespace cast_channel {
namespace {

const char* const kParseErrorPrefix = "Failed to parse auth message: ";

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

}  // namespace

AuthResult::AuthResult() : error_type(ERROR_NONE), nss_error_code(0) {
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

  return AuthResult();
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions
