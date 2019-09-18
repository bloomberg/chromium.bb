// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CHANNEL_CAST_AUTH_UTIL_H_
#define CAST_SENDER_CHANNEL_CAST_AUTH_UTIL_H_

#include <openssl/x509.h>

#include <string>

#include "cast/common/certificate/cast_cert_validator.h"
#include "cast/sender/channel/proto/cast_channel.pb.h"
#include "platform/base/error.h"

namespace cast {
namespace certificate {
enum class CRLPolicy;
struct DateTime;
struct TrustStore;
}  // namespace certificate
}  // namespace cast

namespace cast {
namespace channel {

class AuthResponse;
class CastMessage;

using openscreen::ErrorOr;
using CastDeviceCertPolicy = certificate::CastDeviceCertPolicy;

class AuthContext {
 public:
  ~AuthContext();

  // Get an auth challenge context.
  // The same context must be used in the challenge and reply.
  static AuthContext Create();

  // Verifies the nonce received in the response is equivalent to the one sent.
  // Returns success if |nonce_response| matches nonce_
  openscreen::Error VerifySenderNonce(
      const std::string& nonce_response,
      bool enforce_nonce_checking = false) const;

  // The nonce challenge.
  const std::string& nonce() const { return nonce_; }

 private:
  explicit AuthContext(const std::string& nonce);

  const std::string nonce_;
};

// Authenticates the given |challenge_reply|:
// 1. Signature contained in the reply is valid.
// 2. Certficate used to sign is rooted to a trusted CA.
ErrorOr<CastDeviceCertPolicy> AuthenticateChallengeReply(
    const CastMessage& challenge_reply,
    X509* peer_cert,
    const AuthContext& auth_context);

// Performs a quick check of the TLS certificate for time validity requirements.
openscreen::Error VerifyTLSCertificateValidity(
    X509* peer_cert,
    std::chrono::seconds verification_time);

// Auth-library specific implementation of cryptographic signature verification
// routines. Verifies that |response| contains a valid signature of
// |signature_input|.
ErrorOr<CastDeviceCertPolicy> VerifyCredentials(
    const AuthResponse& response,
    const std::string& signature_input,
    bool enforce_revocation_checking = false,
    bool enforce_sha256_checking = false);

// Exposed for testing only.
//
// Overloaded version of VerifyCredentials that allows modifying
// the crl policy, trust stores, and verification times.
ErrorOr<CastDeviceCertPolicy> VerifyCredentialsForTest(
    const AuthResponse& response,
    const std::string& signature_input,
    certificate::CRLPolicy crl_policy,
    certificate::TrustStore* cast_trust_store,
    certificate::TrustStore* crl_trust_store,
    const certificate::DateTime& verification_time,
    bool enforce_sha256_checking = false);

}  // namespace channel
}  // namespace cast

#endif  // CAST_SENDER_CHANNEL_CAST_AUTH_UTIL_H_
