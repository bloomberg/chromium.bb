// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_

#include <string>

namespace extensions {
namespace core_api {
namespace cast_channel {

class CastMessage;

struct AuthResult {
 public:
  enum ErrorType {
    ERROR_NONE,
    ERROR_PEER_CERT_EMPTY,
    ERROR_WRONG_PAYLOAD_TYPE,
    ERROR_NO_PAYLOAD,
    ERROR_PAYLOAD_PARSING_FAILED,
    ERROR_MESSAGE_ERROR,
    ERROR_NO_RESPONSE,
    ERROR_FINGERPRINT_NOT_FOUND,
    ERROR_NSS_CERT_PARSING_FAILED,
    ERROR_NSS_CERT_NOT_SIGNED_BY_TRUSTED_CA,
    ERROR_NSS_CANNOT_EXTRACT_PUBLIC_KEY,
    ERROR_NSS_SIGNED_BLOBS_MISMATCH
  };

  // Constructs a AuthResult that corresponds to success.
  AuthResult();
  ~AuthResult();

  static AuthResult Create(const std::string& error_message,
                           ErrorType error_type);
  static AuthResult CreateWithNSSError(const std::string& error_message,
                                       ErrorType error_type,
                                       int nss_error_code);

  bool success() const { return error_type == ERROR_NONE; }

  std::string error_message;
  ErrorType error_type;
  int nss_error_code;

 private:
  AuthResult(const std::string& error_message,
             ErrorType error_type,
             int nss_error_code);
};

// Authenticates the given |challenge_reply|:
// 1. Signature contained in the reply is valid.
// 2. Certficate used to sign is rooted to a trusted CA.
AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const std::string& peer_cert);

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_UTIL_H_
