// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/cast_auth_util.h"

#include <openssl/rand.h>

#include <vector>

#include "cast/common/certificate/cast_cert_validator.h"
#include "cast/common/certificate/cast_cert_validator_internal.h"
#include "cast/common/certificate/cast_crl.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "platform/base/error.h"

namespace cast {
namespace channel {
namespace {

#define PARSE_ERROR_PREFIX "Failed to parse auth message: "

// The maximum number of days a cert can live for.
const int kMaxSelfSignedCertLifetimeInDays = 4;

// The size of the nonce challenge in bytes.
const int kNonceSizeInBytes = 16;

// The number of hours after which a nonce is regenerated.
long kNonceExpirationTimeInHours = 24;

using CastCertError = openscreen::Error::Code;

// Extracts an embedded DeviceAuthMessage payload from an auth challenge reply
// message.
openscreen::Error ParseAuthMessage(const CastMessage& challenge_reply,
                                   DeviceAuthMessage* auth_message) {
  if (challenge_reply.payload_type() != CastMessage_PayloadType_BINARY) {
    return openscreen::Error(CastCertError::kCastV2WrongPayloadType,
                             PARSE_ERROR_PREFIX
                             "Wrong payload type in challenge reply");
  }
  if (!challenge_reply.has_payload_binary()) {
    return openscreen::Error(
        CastCertError::kCastV2NoPayload, PARSE_ERROR_PREFIX
        "Payload type is binary but payload_binary field not set");
  }
  if (!auth_message->ParseFromString(challenge_reply.payload_binary())) {
    return openscreen::Error(
        CastCertError::kCastV2PayloadParsingFailed, PARSE_ERROR_PREFIX
        "Cannot parse binary payload into DeviceAuthMessage");
  }

  if (auth_message->has_error()) {
    std::stringstream ss;
    ss << PARSE_ERROR_PREFIX "Auth message error: "
       << auth_message->error().error_type();
    return openscreen::Error(CastCertError::kCastV2MessageError, ss.str());
  }
  if (!auth_message->has_response()) {
    return openscreen::Error(CastCertError::kCastV2NoResponse,
                             PARSE_ERROR_PREFIX
                             "Auth message has no response field");
  }
  return openscreen::Error::None();
}

class CastNonce {
 public:
  static CastNonce* GetInstance() {
    static CastNonce* cast_nonce = new CastNonce();
    return cast_nonce;
  }

  static const std::string& Get() {
    GetInstance()->EnsureNonceTimely();
    return GetInstance()->nonce_;
  }

 private:
  CastNonce() : nonce_(kNonceSizeInBytes, 0) { GenerateNonce(); }
  void GenerateNonce() {
    OSP_CHECK_EQ(
        RAND_bytes(reinterpret_cast<uint8_t*>(&nonce_[0]), kNonceSizeInBytes),
        1);
    nonce_generation_time_ = openscreen::platform::GetWallTimeSinceUnixEpoch();
  }

  void EnsureNonceTimely() {
    if (openscreen::platform::GetWallTimeSinceUnixEpoch() >
        (nonce_generation_time_ +
         std::chrono::hours(kNonceExpirationTimeInHours))) {
      GenerateNonce();
    }
  }

  // The nonce challenge to send to the Cast receiver.
  // The nonce is updated daily.
  std::string nonce_;
  std::chrono::seconds nonce_generation_time_;
};

// Maps CastCertError from certificate verification to openscreen::Error.
// If crl_required is set to false, all revocation related errors are ignored.
openscreen::Error MapToOpenscreenError(CastCertError error, bool crl_required) {
  switch (error) {
    case CastCertError::kErrCertsMissing:
      return openscreen::Error(CastCertError::kCastV2PeerCertEmpty,
                               "Failed to locate certificates.");
    case CastCertError::kErrCertsParse:
      return openscreen::Error(CastCertError::kErrCertsParse,
                               "Failed to parse certificates.");
    case CastCertError::kErrCertsDateInvalid:
      return openscreen::Error(CastCertError::kCastV2CertNotSignedByTrustedCa,
                               "Failed date validity check.");
    case CastCertError::kErrCertsVerifyGeneric:
      return openscreen::Error(
          CastCertError::kCastV2CertNotSignedByTrustedCa,
          "Failed with a generic certificate verification error.");
    case CastCertError::kErrCertsRestrictions:
      return openscreen::Error(CastCertError::kCastV2CertNotSignedByTrustedCa,
                               "Failed certificate restrictions.");
    case CastCertError::kErrCrlInvalid:
      // This error is only encountered if |crl_required| is true.
      OSP_DCHECK(crl_required);
      return openscreen::Error(CastCertError::kErrCrlInvalid,
                               "Failed to provide a valid CRL.");
    case CastCertError::kErrCertsRevoked:
      return openscreen::Error(CastCertError::kErrCertsRevoked,
                               "Failed certificate revocation check.");
    case CastCertError::kNone:
      return openscreen::Error::None();
    default:
      return openscreen::Error(CastCertError::kCastV2CertNotSignedByTrustedCa,
                               "Failed verifying cast device certificate.");
  }
  return openscreen::Error::None();
}

openscreen::Error VerifyAndMapDigestAlgorithm(
    HashAlgorithm response_digest_algorithm,
    certificate::DigestAlgorithm* digest_algorithm,
    bool enforce_sha256_checking) {
  switch (response_digest_algorithm) {
    case SHA1:
      if (enforce_sha256_checking) {
        return openscreen::Error(CastCertError::kCastV2DigestUnsupported,
                                 "Unsupported digest algorithm.");
      }
      *digest_algorithm = certificate::DigestAlgorithm::kSha1;
      break;
    case SHA256:
      *digest_algorithm = certificate::DigestAlgorithm::kSha256;
      break;
    default:
      return CastCertError::kCastV2DigestUnsupported;
  }
  return openscreen::Error::None();
}

}  // namespace

// static
AuthContext AuthContext::Create() {
  return AuthContext(CastNonce::Get());
}

AuthContext::AuthContext(const std::string& nonce) : nonce_(nonce) {}

AuthContext::~AuthContext() {}

openscreen::Error AuthContext::VerifySenderNonce(
    const std::string& nonce_response,
    bool enforce_nonce_checking) const {
  if (nonce_ != nonce_response) {
    if (enforce_nonce_checking) {
      return openscreen::Error(CastCertError::kCastV2SenderNonceMismatch,
                               "Sender nonce mismatched.");
    }
  }
  return openscreen::Error::None();
}

openscreen::Error VerifyTLSCertificateValidity(
    X509* peer_cert,
    std::chrono::seconds verification_time) {
  // Ensure the peer cert is valid and doesn't have an excessive remaining
  // lifetime. Although it is not verified as an X.509 certificate, the entire
  // structure is signed by the AuthResponse, so the validity field from X.509
  // is repurposed as this signature's expiration.
  certificate::DateTime not_before;
  certificate::DateTime not_after;
  if (!certificate::GetCertValidTimeRange(peer_cert, &not_before, &not_after)) {
    return openscreen::Error(CastCertError::kErrCertsParse, PARSE_ERROR_PREFIX
                             "Parsing validity fields failed.");
  }

  std::chrono::seconds lifetime_limit =
      verification_time +
      std::chrono::hours(24 * kMaxSelfSignedCertLifetimeInDays);
  certificate::DateTime verification_time_exploded = {};
  certificate::DateTime lifetime_limit_exploded = {};
  OSP_CHECK(certificate::DateTimeFromSeconds(verification_time.count(),
                                             &verification_time_exploded));
  OSP_CHECK(certificate::DateTimeFromSeconds(lifetime_limit.count(),
                                             &lifetime_limit_exploded));
  if (verification_time_exploded < not_before) {
    return openscreen::Error(
        CastCertError::kCastV2TlsCertValidStartDateInFuture,
        PARSE_ERROR_PREFIX "Certificate's valid start date is in the future.");
  }
  if (not_after < verification_time_exploded) {
    return openscreen::Error(CastCertError::kCastV2TlsCertExpired,
                             PARSE_ERROR_PREFIX "Certificate has expired.");
  }
  if (lifetime_limit_exploded < not_after) {
    return openscreen::Error(CastCertError::kCastV2TlsCertValidityPeriodTooLong,
                             PARSE_ERROR_PREFIX
                             "Peer cert lifetime is too long.");
  }
  return openscreen::Error::None();
}

ErrorOr<CastDeviceCertPolicy> AuthenticateChallengeReply(
    const CastMessage& challenge_reply,
    X509* peer_cert,
    const AuthContext& auth_context) {
  DeviceAuthMessage auth_message;
  openscreen::Error result = ParseAuthMessage(challenge_reply, &auth_message);
  if (!result.ok()) {
    return result;
  }

  result = VerifyTLSCertificateValidity(
      peer_cert, openscreen::platform::GetWallTimeSinceUnixEpoch());
  if (!result.ok()) {
    return result;
  }

  const AuthResponse& response = auth_message.response();
  const std::string& nonce_response = response.sender_nonce();

  result = auth_context.VerifySenderNonce(nonce_response);
  if (!result.ok()) {
    return result;
  }

  int len = i2d_X509(peer_cert, nullptr);
  if (len <= 0) {
    return openscreen::Error(CastCertError::kErrCertsParse,
                             "Serializing cert failed.");
  }
  std::string peer_cert_der(len, 0);
  uint8_t* data = reinterpret_cast<uint8_t*>(&peer_cert_der[0]);
  if (!i2d_X509(peer_cert, &data)) {
    return openscreen::Error(CastCertError::kErrCertsParse,
                             "Serializing cert failed.");
  }
  size_t actual_size = data - reinterpret_cast<uint8_t*>(&peer_cert_der[0]);
  OSP_DCHECK_EQ(actual_size, peer_cert_der.size());
  peer_cert_der.resize(actual_size);

  return VerifyCredentials(response, nonce_response + peer_cert_der);
}

// This function does the following
//
// * Verifies that the certificate chain |response.client_auth_certificate| +
//   |response.intermediate_certificate| is valid and chains to a trusted Cast
//   root. The list of trusted Cast roots can be overrided by providing a
//   non-nullptr |cast_trust_store|. The certificate is verified at
//   |verification_time|.
//
// * Verifies that none of the certificates in the chain are revoked based on
//   the CRL provided in the response |response.crl|. The CRL is verified to be
//   valid and its issuer certificate chains to a trusted Cast CRL root. The
//   list of trusted Cast CRL roots can be overrided by providing a non-nullptr
//   |crl_trust_store|. If |crl_policy| is kCrlOptional then the result of
//   revocation checking is ignored. The CRL is verified at |verification_time|.
//
// * Verifies that |response.signature| matches the signature of
//   |signature_input| by |response.client_auth_certificate|'s public key.
ErrorOr<CastDeviceCertPolicy> VerifyCredentialsImpl(
    const AuthResponse& response,
    const std::string& signature_input,
    const certificate::CRLPolicy& crl_policy,
    certificate::TrustStore* cast_trust_store,
    certificate::TrustStore* crl_trust_store,
    const certificate::DateTime& verification_time,
    bool enforce_sha256_checking) {
  if (response.signature().empty() && !signature_input.empty()) {
    return openscreen::Error(CastCertError::kCastV2SignatureEmpty,
                             "Signature is empty.");
  }

  // Verify the certificate
  std::unique_ptr<certificate::CertVerificationContext> verification_context;

  // Build a single vector containing the certificate chain.
  std::vector<std::string> cert_chain;
  cert_chain.push_back(response.client_auth_certificate());
  cert_chain.insert(cert_chain.end(),
                    response.intermediate_certificate().begin(),
                    response.intermediate_certificate().end());

  // Parse the CRL.
  std::unique_ptr<certificate::CastCRL> crl;
  if (!response.crl().empty()) {
    crl = certificate::ParseAndVerifyCRL(response.crl(), verification_time,
                                         crl_trust_store);
  }

  // Perform certificate verification.
  certificate::CastDeviceCertPolicy device_policy;
  openscreen::Error verify_result = certificate::VerifyDeviceCert(
      cert_chain, verification_time, &verification_context, &device_policy,
      crl.get(), crl_policy, cast_trust_store);

  // Handle and report errors.
  openscreen::Error result = MapToOpenscreenError(
      verify_result.code(), crl_policy == certificate::CRLPolicy::kCrlRequired);
  if (!result.ok()) {
    return result;
  }

  // The certificate is verified at this point.
  certificate::DigestAlgorithm digest_algorithm;
  openscreen::Error digest_result = VerifyAndMapDigestAlgorithm(
      response.hash_algorithm(), &digest_algorithm, enforce_sha256_checking);
  if (!digest_result.ok()) {
    return digest_result;
  }

  certificate::ConstDataSpan signature = {
      reinterpret_cast<const uint8_t*>(response.signature().data()),
      static_cast<uint32_t>(response.signature().size())};
  certificate::ConstDataSpan siginput = {
      reinterpret_cast<const uint8_t*>(signature_input.data()),
      static_cast<uint32_t>(signature_input.size())};
  if (!verification_context->VerifySignatureOverData(signature, siginput,
                                                     digest_algorithm)) {
    return openscreen::Error(CastCertError::kCastV2SignedBlobsMismatch,
                             "Failed verifying signature over data.");
  }

  return device_policy;
}

ErrorOr<CastDeviceCertPolicy> VerifyCredentials(
    const AuthResponse& response,
    const std::string& signature_input,
    bool enforce_revocation_checking,
    bool enforce_sha256_checking) {
  certificate::DateTime now = {};
  OSP_CHECK(certificate::DateTimeFromSeconds(
      openscreen::platform::GetWallTimeSinceUnixEpoch().count(), &now));
  certificate::CRLPolicy policy = (enforce_revocation_checking)
                                      ? certificate::CRLPolicy::kCrlRequired
                                      : certificate::CRLPolicy::kCrlOptional;
  return VerifyCredentialsImpl(response, signature_input, policy, nullptr,
                               nullptr, now, enforce_sha256_checking);
}

ErrorOr<CastDeviceCertPolicy> VerifyCredentialsForTest(
    const AuthResponse& response,
    const std::string& signature_input,
    certificate::CRLPolicy crl_policy,
    certificate::TrustStore* cast_trust_store,
    certificate::TrustStore* crl_trust_store,
    const certificate::DateTime& verification_time,
    bool enforce_sha256_checking) {
  return VerifyCredentialsImpl(response, signature_input, crl_policy,
                               cast_trust_store, crl_trust_store,
                               verification_time, enforce_sha256_checking);
}

}  // namespace channel
}  // namespace cast
