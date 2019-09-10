// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/cast_auth_util.h"

#include <string>

#include "cast/common/certificate/cast_cert_validator.h"
#include "cast/common/certificate/cast_crl.h"
#include "cast/common/certificate/proto/test_suite.pb.h"
#include "cast/common/certificate/test_helpers.h"
#include "cast/sender/channel/proto/cast_channel.pb.h"
#include "gtest/gtest.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"

namespace cast {
namespace channel {
namespace {

using ErrorCode = openscreen::Error::Code;

bool ConvertTimeSeconds(const certificate::DateTime& time, uint64_t* seconds) {
  static constexpr uint64_t kDaysPerYear = 365;
  static constexpr uint64_t kHoursPerDay = 24;
  static constexpr uint64_t kMinutesPerHour = 60;
  static constexpr uint64_t kSecondsPerMinute = 60;

  static constexpr uint64_t kSecondsPerDay =
      kSecondsPerMinute * kMinutesPerHour * kHoursPerDay;
  static constexpr uint64_t kDaysPerQuadYear = 4 * kDaysPerYear + 1;
  static constexpr uint64_t kDaysPerCentury =
      kDaysPerQuadYear * 24 + kDaysPerYear * 4;
  static constexpr uint64_t kDaysPerQuadCentury = 4 * kDaysPerCentury + 1;

  static constexpr uint64_t kDaysPerMonth[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };

  bool is_leap_year =
      (time.year % 4 == 0 && (time.year % 100 != 0 || time.year % 400 == 0));
  if (time.year < 1970 || time.month < 1 || time.day < 1 ||
      time.day > (kDaysPerMonth[time.month - 1] + is_leap_year) ||
      time.month > 12 || time.hour > 23 || time.minute > 59 ||
      time.second > 60) {
    return false;
  }
  uint64_t result = 0;
  uint64_t year = time.year - 1970;
  uint64_t first_two_years = year >= 2;
  result += first_two_years * 2 * kDaysPerYear * kSecondsPerDay;
  year -= first_two_years * 2;

  if (first_two_years) {
    uint64_t twenty_eight_years = year >= 28;
    result += twenty_eight_years * 7 * kDaysPerQuadYear * kSecondsPerDay;
    year -= twenty_eight_years * 28;

    if (twenty_eight_years) {
      uint64_t quad_centuries = year / 400;
      result += quad_centuries * kDaysPerQuadCentury * kSecondsPerDay;
      year -= quad_centuries * 400;

      uint64_t first_century = year >= 100;
      result += first_century * (kDaysPerCentury + 1) * kSecondsPerDay;
      year -= first_century * 100;

      uint64_t centuries = year / 100;
      result += centuries * kDaysPerCentury * kSecondsPerDay;
      year -= centuries * 100;
    }

    uint64_t quad_years = year / 4;
    result += quad_years * kDaysPerQuadYear * kSecondsPerDay;
    year -= quad_years * 4;

    uint64_t first_year = year >= 1;
    result += first_year * (kDaysPerYear + 1) * kSecondsPerDay;
    year -= first_year;

    result += year * kDaysPerYear * kSecondsPerDay;
    OSP_DCHECK_LE(year, 2);
  }

  for (int i = 0; i < time.month - 1; ++i) {
    uint64_t days = kDaysPerMonth[i];
    result += days * kSecondsPerDay;
  }
  if (time.month >= 3 && is_leap_year) {
    result += kSecondsPerDay;
  }
  result += (time.day - 1) * kSecondsPerDay;
  result += time.hour * kMinutesPerHour * kSecondsPerMinute;
  result += time.minute * kSecondsPerMinute;
  result += time.second;

  *seconds = result;
  return true;
}

#define TEST_DATA_PREFIX "test/data/cast/common/certificate/"

class CastAuthUtilTest : public testing::Test {
 public:
  CastAuthUtilTest() {}
  ~CastAuthUtilTest() override {}

  void SetUp() override {}

 protected:
  static AuthResponse CreateAuthResponse(std::string* signed_data,
                                         HashAlgorithm digest_algorithm) {
    std::vector<std::string> chain =
        certificate::testing::ReadCertificatesFromPemFile(
            TEST_DATA_PREFIX "certificates/chromecast_gen1.pem");
    OSP_CHECK(!chain.empty());

    certificate::testing::SignatureTestData signatures =
        certificate::testing::ReadSignatureTestData(
            TEST_DATA_PREFIX "signeddata/2ZZBG9_FA8FCA3EF91A.pem");

    AuthResponse response;

    response.set_client_auth_certificate(chain[0]);
    for (size_t i = 1; i < chain.size(); ++i)
      response.add_intermediate_certificate(chain[i]);

    response.set_hash_algorithm(digest_algorithm);
    switch (digest_algorithm) {
      case SHA1:
        response.set_signature(
            std::string(reinterpret_cast<const char*>(signatures.sha1.data),
                        signatures.sha1.length));
        break;
      case SHA256:
        response.set_signature(
            std::string(reinterpret_cast<const char*>(signatures.sha256.data),
                        signatures.sha256.length));
        break;
    }
    signed_data->assign(reinterpret_cast<const char*>(signatures.message.data),
                        signatures.message.length);

    return response;
  }

  // Mangles a string by inverting the first byte.
  static void MangleString(std::string* str) { (*str)[0] = ~(*str)[0]; }
};

// Note on expiration: VerifyCredentials() depends on the system clock. In
// practice this shouldn't be a problem though since the certificate chain
// being verified doesn't expire until 2032.
TEST_F(CastAuthUtilTest, VerifySuccess) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  certificate::DateTime now = {};
  ASSERT_TRUE(certificate::DateTimeFromSeconds(
      openscreen::platform::GetWallTimeSinceUnixEpoch().count(), &now));
  ErrorOr<CastDeviceCertPolicy> result = VerifyCredentialsForTest(
      auth_response, signed_data, certificate::CRLPolicy::kCrlOptional, nullptr,
      nullptr, now);
  EXPECT_TRUE(result);
  EXPECT_EQ(certificate::CastDeviceCertPolicy::kUnrestricted, result.value());
}

TEST_F(CastAuthUtilTest, VerifyBadCA) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_intermediate_certificate(0));
  ErrorOr<CastDeviceCertPolicy> result =
      VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kErrCertsParse, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifyBadClientAuthCert) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_client_auth_certificate());
  ErrorOr<CastDeviceCertPolicy> result =
      VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kErrCertsParse, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifyBadSignature) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_signature());
  ErrorOr<CastDeviceCertPolicy> result =
      VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2SignedBlobsMismatch, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifyEmptySignature) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  auth_response.mutable_signature()->clear();
  ErrorOr<CastDeviceCertPolicy> result =
      VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2SignatureEmpty, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifyUnsupportedDigest) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA1);
  certificate::DateTime now = {};
  ASSERT_TRUE(certificate::DateTimeFromSeconds(
      openscreen::platform::GetWallTimeSinceUnixEpoch().count(), &now));
  ErrorOr<CastDeviceCertPolicy> result = VerifyCredentialsForTest(
      auth_response, signed_data, certificate::CRLPolicy::kCrlOptional, nullptr,
      nullptr, now, true);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2DigestUnsupported, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifyBackwardsCompatibleDigest) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA1);
  certificate::DateTime now = {};
  ASSERT_TRUE(certificate::DateTimeFromSeconds(
      openscreen::platform::GetWallTimeSinceUnixEpoch().count(), &now));
  ErrorOr<CastDeviceCertPolicy> result = VerifyCredentialsForTest(
      auth_response, signed_data, certificate::CRLPolicy::kCrlOptional, nullptr,
      nullptr, now);
  EXPECT_TRUE(result);
}

TEST_F(CastAuthUtilTest, VerifyBadPeerCert) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(&signed_data);
  ErrorOr<CastDeviceCertPolicy> result =
      VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2SignedBlobsMismatch, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifySenderNonceMatch) {
  AuthContext context = AuthContext::Create();
  ErrorOr<CastDeviceCertPolicy> result =
      context.VerifySenderNonce(context.nonce(), true);
  EXPECT_TRUE(result);
}

TEST_F(CastAuthUtilTest, VerifySenderNonceMismatch) {
  AuthContext context = AuthContext::Create();
  std::string received_nonce = "test2";
  EXPECT_NE(received_nonce, context.nonce());
  ErrorOr<CastDeviceCertPolicy> result =
      context.VerifySenderNonce(received_nonce, true);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2SenderNonceMismatch, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifySenderNonceMissing) {
  AuthContext context = AuthContext::Create();
  std::string received_nonce;
  EXPECT_FALSE(context.nonce().empty());
  ErrorOr<CastDeviceCertPolicy> result =
      context.VerifySenderNonce(received_nonce, true);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2SenderNonceMismatch, result.error().code());
}

TEST_F(CastAuthUtilTest, VerifyTLSCertificateSuccess) {
  std::vector<std::string> tls_cert_der =
      certificate::testing::ReadCertificatesFromPemFile(
          TEST_DATA_PREFIX "certificates/test_tls_cert.pem");
  std::string& der_cert = tls_cert_der[0];
  const uint8_t* data = (const uint8_t*)der_cert.data();
  X509* tls_cert = d2i_X509(nullptr, &data, der_cert.size());
  certificate::DateTime not_before;
  certificate::DateTime not_after;
  ASSERT_TRUE(
      certificate::GetCertValidTimeRange(tls_cert, &not_before, &not_after));
  uint64_t x;
  ASSERT_TRUE(ConvertTimeSeconds(not_before, &x));
  std::chrono::seconds s(x);

  ErrorOr<CastDeviceCertPolicy> result =
      VerifyTLSCertificateValidity(tls_cert, s);
  EXPECT_TRUE(result);
  X509_free(tls_cert);
}

TEST_F(CastAuthUtilTest, VerifyTLSCertificateTooEarly) {
  std::vector<std::string> tls_cert_der =
      certificate::testing::ReadCertificatesFromPemFile(
          TEST_DATA_PREFIX "certificates/test_tls_cert.pem");
  std::string& der_cert = tls_cert_der[0];
  const uint8_t* data = (const uint8_t*)der_cert.data();
  X509* tls_cert = d2i_X509(nullptr, &data, der_cert.size());
  certificate::DateTime not_before;
  certificate::DateTime not_after;
  ASSERT_TRUE(
      certificate::GetCertValidTimeRange(tls_cert, &not_before, &not_after));
  uint64_t x;
  ASSERT_TRUE(ConvertTimeSeconds(not_before, &x));
  std::chrono::seconds s(x - 1);

  ErrorOr<CastDeviceCertPolicy> result =
      VerifyTLSCertificateValidity(tls_cert, s);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2TlsCertValidStartDateInFuture,
            result.error().code());
  X509_free(tls_cert);
}

TEST_F(CastAuthUtilTest, VerifyTLSCertificateTooLate) {
  std::vector<std::string> tls_cert_der =
      certificate::testing::ReadCertificatesFromPemFile(
          TEST_DATA_PREFIX "certificates/test_tls_cert.pem");
  std::string& der_cert = tls_cert_der[0];
  const uint8_t* data = (const uint8_t*)der_cert.data();
  X509* tls_cert = d2i_X509(nullptr, &data, der_cert.size());
  certificate::DateTime not_before;
  certificate::DateTime not_after;
  ASSERT_TRUE(
      certificate::GetCertValidTimeRange(tls_cert, &not_before, &not_after));
  uint64_t x;
  ASSERT_TRUE(ConvertTimeSeconds(not_after, &x));
  std::chrono::seconds s(x + 2);

  ErrorOr<CastDeviceCertPolicy> result =
      VerifyTLSCertificateValidity(tls_cert, s);
  EXPECT_FALSE(result);
  EXPECT_EQ(ErrorCode::kCastV2TlsCertExpired, result.error().code());
  X509_free(tls_cert);
}

// Indicates the expected result of test step's verification.
enum TestStepResult {
  RESULT_SUCCESS,
  RESULT_FAIL,
};

// Verifies that the certificate chain provided is not revoked according to
// the provided Cast CRL at |verification_time|.
// The provided CRL is verified at |verification_time|.
// If |crl_required| is set, then a valid Cast CRL must be provided.
// Otherwise, a missing CRL is be ignored.
ErrorOr<CastDeviceCertPolicy> TestVerifyRevocation(
    const std::vector<std::string>& certificate_chain,
    const std::string& crl_bundle,
    const certificate::DateTime& verification_time,
    bool crl_required,
    certificate::TrustStore* cast_trust_store,
    certificate::TrustStore* crl_trust_store) {
  AuthResponse response;

  if (certificate_chain.size() > 0) {
    response.set_client_auth_certificate(certificate_chain[0]);
    for (size_t i = 1; i < certificate_chain.size(); ++i)
      response.add_intermediate_certificate(certificate_chain[i]);
  }

  response.set_crl(crl_bundle);

  certificate::CRLPolicy crl_policy = certificate::CRLPolicy::kCrlRequired;
  if (!crl_required && crl_bundle.empty())
    crl_policy = certificate::CRLPolicy::kCrlOptional;
  ErrorOr<CastDeviceCertPolicy> result =
      VerifyCredentialsForTest(response, "", crl_policy, cast_trust_store,
                               crl_trust_store, verification_time);
  // This test doesn't set the signature so it will just fail there.
  EXPECT_FALSE(result);
  return result;
}

// Runs a single test case.
bool RunTest(const certificate::DeviceCertTest& test_case) {
  std::unique_ptr<certificate::TrustStore> crl_trust_store;
  std::unique_ptr<certificate::TrustStore> cast_trust_store;
  if (test_case.use_test_trust_anchors()) {
    crl_trust_store = certificate::testing::CreateTrustStoreFromPemFile(
        TEST_DATA_PREFIX "certificates/cast_crl_test_root_ca.pem");
    cast_trust_store = certificate::testing::CreateTrustStoreFromPemFile(
        TEST_DATA_PREFIX "certificates/cast_test_root_ca.pem");

    EXPECT_FALSE(crl_trust_store->certs.empty());
    EXPECT_FALSE(cast_trust_store->certs.empty());
  }

  std::vector<std::string> certificate_chain;
  for (auto const& cert : test_case.der_cert_path()) {
    certificate_chain.push_back(cert);
  }

  // CastAuthUtil verifies the CRL at the same time as the certificate.
  certificate::DateTime verification_time;
  uint64_t cert_verify_time = test_case.cert_verification_time_seconds();
  if (!cert_verify_time) {
    cert_verify_time = test_case.crl_verification_time_seconds();
  }
  OSP_DCHECK(
      certificate::DateTimeFromSeconds(cert_verify_time, &verification_time));

  std::string crl_bundle = test_case.crl_bundle();
  ErrorOr<CastDeviceCertPolicy> result(
      certificate::CastDeviceCertPolicy::kUnrestricted);
  switch (test_case.expected_result()) {
    case certificate::PATH_VERIFICATION_FAILED:
      result = TestVerifyRevocation(
          certificate_chain, crl_bundle, verification_time, false,
          cast_trust_store.get(), crl_trust_store.get());
      EXPECT_EQ(result.error().code(),
                ErrorCode::kCastV2CertNotSignedByTrustedCa);
      return result.error().code() ==
             ErrorCode::kCastV2CertNotSignedByTrustedCa;
    case certificate::CRL_VERIFICATION_FAILED:
    // Fall-through intended.
    case certificate::REVOCATION_CHECK_FAILED_WITHOUT_CRL:
      result = TestVerifyRevocation(
          certificate_chain, crl_bundle, verification_time, true,
          cast_trust_store.get(), crl_trust_store.get());
      EXPECT_EQ(result.error().code(), ErrorCode::kErrCrlInvalid);
      return result.error().code() == ErrorCode::kErrCrlInvalid;
    case certificate::CRL_EXPIRED_AFTER_INITIAL_VERIFICATION:
      // By-pass this test because CRL is always verified at the time the
      // certificate is verified.
      return true;
    case certificate::REVOCATION_CHECK_FAILED:
      result = TestVerifyRevocation(
          certificate_chain, crl_bundle, verification_time, true,
          cast_trust_store.get(), crl_trust_store.get());
      EXPECT_EQ(result.error().code(), ErrorCode::kErrCertsRevoked);
      return result.error().code() == ErrorCode::kErrCertsRevoked;
    case certificate::SUCCESS:
      result = TestVerifyRevocation(
          certificate_chain, crl_bundle, verification_time, false,
          cast_trust_store.get(), crl_trust_store.get());
      EXPECT_EQ(result.error().code(), ErrorCode::kCastV2SignedBlobsMismatch);
      return result.error().code() == ErrorCode::kCastV2SignedBlobsMismatch;
    case certificate::UNSPECIFIED:
      return false;
  }
  return false;
}

// Parses the provided test suite provided in wire-format proto.
// Each test contains the inputs and the expected output.
// To see the description of the test, execute the test.
// These tests are generated by a test generator in google3.
void RunTestSuite(const std::string& test_suite_file_name) {
  std::string testsuite_raw =
      certificate::testing::ReadEntireFileToString(test_suite_file_name);
  certificate::DeviceCertTestSuite test_suite;
  EXPECT_TRUE(test_suite.ParseFromString(testsuite_raw));
  uint16_t successes = 0;

  for (auto const& test_case : test_suite.tests()) {
    bool result = RunTest(test_case);
    EXPECT_TRUE(result) << test_case.description();
    successes += result;
  }
  OSP_LOG_IF(ERROR, successes != test_suite.tests().size())
      << "successes: " << successes
      << ", failures: " << (test_suite.tests().size() - successes);
}

TEST_F(CastAuthUtilTest, CRLTestSuite) {
  RunTestSuite("testsuite/testsuite1.pb");
}

}  // namespace
}  // namespace channel
}  // namespace cast
