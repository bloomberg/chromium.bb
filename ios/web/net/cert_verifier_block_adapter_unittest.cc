// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/cert_verifier_block_adapter.h"

#include "base/message_loop/message_loop.h"
#include "base/test/ios/wait_util.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log.h"
#include "net/test/cert_test_util.h"
#include "testing/platform_test.h"

namespace web {

namespace {
// Test cert filename.
const char kCertFileName[] = "ok_cert.pem";
// Test hostname for CertVerifier.
const char kHostName[] = "www.example.com";

}  // namespace

// Test fixture to test CertVerifierBlockAdapter class.
class CertVerifierBlockAdapterTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kCertFileName);
    ASSERT_TRUE(cert_);
  }

  // Performs synchronous verification.
  void Verify(CertVerifierBlockAdapter* cert_verifier_adapter,
              CertVerifierBlockAdapter::Params params,
              net::CertVerifyResult* result,
              int* error) {
    __block bool verification_completed = false;
    cert_verifier_adapter->Verify(
        params, ^(net::CertVerifyResult callback_result, int callback_error) {
          *result = callback_result;
          *error = callback_error;
          verification_completed = true;
        });
    base::test::ios::WaitUntilCondition(^{
      return verification_completed;
    }, base::MessageLoop::current(), base::TimeDelta());
  }

  web::TestWebThreadBundle thread_bundle_;
  scoped_refptr<net::X509Certificate> cert_;
  net::NetLog net_log_;
};

// Tests |Verify| with default params and synchronous verification.
TEST_F(CertVerifierBlockAdapterTest, DefaultParamsAndSync) {
  // Set up verifier mock.
  net::MockCertVerifier verifier;
  CertVerifierBlockAdapter test_adapter(&verifier, &net_log_);
  const int kExpectedError = net::ERR_CERT_AUTHORITY_INVALID;
  net::CertVerifyResult expected_result;
  expected_result.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  expected_result.verified_cert = cert_;
  verifier.AddResultForCertAndHost(cert_.get(), kHostName, expected_result,
                                   kExpectedError);

  // Call |Verify|.
  net::CertVerifyResult actual_result;
  int actual_error = -1;
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  Verify(&test_adapter, params, &actual_result, &actual_error);

  // Ensure that Verification results are correct.
  EXPECT_EQ(kExpectedError, actual_error);
  EXPECT_EQ(expected_result.cert_status, actual_result.cert_status);
}

// Tests |Verify| with default params and asynchronous verification using real
// net::CertVerifier and ok_cert.pem cert.
TEST_F(CertVerifierBlockAdapterTest, DefaultParamsAndAsync) {
  // Call |Verify|.
  scoped_ptr<net::CertVerifier> verifier(net::CertVerifier::CreateDefault());
  CertVerifierBlockAdapter test_adapter(verifier.get(), &net_log_);
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  net::CertVerifyResult actual_result;
  int actual_error = -1;
  Verify(&test_adapter, params, &actual_result, &actual_error);

  // Ensure that Verification results are correct.
  EXPECT_FALSE(actual_result.is_issued_by_known_root);
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, actual_error);
}

// Tests |Verify| with invalid cert argument.
TEST_F(CertVerifierBlockAdapterTest, InvalidCert) {
  // Call |Verify|.
  net::MockCertVerifier verifier;
  CertVerifierBlockAdapter test_adapter(&verifier, &net_log_);
  net::CertVerifyResult actual_result;
  int actual_error = -1;
  CertVerifierBlockAdapter::Params params(nullptr, kHostName);
  Verify(&test_adapter, params, &actual_result, &actual_error);

  // Ensure that Verification results are correct.
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, actual_error);
}

// Tests |Verify| with invalid hostname argument.
TEST_F(CertVerifierBlockAdapterTest, InvalidHostname) {
  // Call |Verify|.
  net::MockCertVerifier verifier;
  CertVerifierBlockAdapter test_adapter(&verifier, &net_log_);
  net::CertVerifyResult actual_result;
  int actual_error = -1;
  CertVerifierBlockAdapter::Params params(cert_.get(), std::string());
  Verify(&test_adapter, params, &actual_result, &actual_error);

  // Ensure that Verification results are correct.
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, actual_error);
}

// Tests |Verify| with synchronous error.
TEST_F(CertVerifierBlockAdapterTest, DefaultParamsAndSyncError) {
  // Set up expectation.
  net::MockCertVerifier verifier;
  CertVerifierBlockAdapter test_adapter(&verifier, &net_log_);
  const int kExpectedError = net::ERR_INSUFFICIENT_RESOURCES;
  net::CertVerifyResult expected_result;
  expected_result.verified_cert = cert_;
  verifier.AddResultForCertAndHost(cert_.get(), kHostName, expected_result,
                                   kExpectedError);

  // Call |Verify|.
  net::CertVerifyResult actual_result;
  int actual_error = -1;
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  Verify(&test_adapter, params, &actual_result, &actual_error);

  // Ensure that Verification results are correct.
  EXPECT_EQ(kExpectedError, actual_error);
}

// Tests that the completion handler passed to |Verify()| is called, even if the
// adapter is destroyed.
TEST_F(CertVerifierBlockAdapterTest, CompletionHandlerIsAlwaysCalled) {
  scoped_ptr<net::CertVerifier> verifier(net::CertVerifier::CreateDefault());
  scoped_ptr<CertVerifierBlockAdapter> test_adapter(
      new CertVerifierBlockAdapter(verifier.get(), &net_log_));

  // Call |Verify| and destroy the adapter.
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  __block bool verification_completed = false;
  test_adapter->Verify(params, ^(net::CertVerifyResult, int) {
    verification_completed = true;
  });
  test_adapter.reset();

  // Make sure that the completion handler is called.
  base::test::ios::WaitUntilCondition(^{
    return verification_completed;
  }, base::MessageLoop::current(), base::TimeDelta());
}

}  // namespace web
