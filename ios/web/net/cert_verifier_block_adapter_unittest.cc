// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/cert_verifier_block_adapter.h"

#include "base/test/ios/wait_util.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace net {

using testing::_;

namespace {

// Test hostname for CertVerifier.
const char kHostName[] = "chromium.org";
// Test OCSP response for CertVerifier.
const char kOcspResponse[] = "ocsp";

// Mocks CertVerifier for CertVerifierBlockAdapter testing.
class CertVerifierMock : public CertVerifier {
 public:
  MOCK_METHOD9(Verify,
               int(X509Certificate* cert,
                   const std::string& hostname,
                   const std::string& ocsp_response,
                   int flags,
                   CRLSet* crl_set,
                   CertVerifyResult* verify_result,
                   const CompletionCallback& callback,
                   scoped_ptr<Request>* out_req,
                   const BoundNetLog& net_log));
};

// Sets CertVerifyResult to emulate CertVerifier behavior.
ACTION_P(SetVerifyResult, result) {
  *arg5 = result;
}

// Calls CompletionCallback to emulate CertVerifier behavior.
ACTION(RunCallback) {
  arg6.Run(0);
}

}  // namespace

// Test fixture to test CertVerifierBlockAdapter class.
class CertVerifierBlockAdapterTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    cert_ = new X509Certificate("test", "test", base::Time(), base::Time());
    scoped_ptr<CertVerifierMock> cert_verifier_mock(new CertVerifierMock());
    cert_verifier_mock_ = cert_verifier_mock.get();
    test_adapter_.reset(
        new CertVerifierBlockAdapter(cert_verifier_mock.Pass()));
  }

  // Performs synchronous verification.
  void Verify(CertVerifierBlockAdapter::Params params,
              scoped_ptr<net::CertVerifyResult>* result,
              int* status) {
    __block bool verification_completed = false;
    test_adapter_->Verify(params,
                          ^(scoped_ptr<net::CertVerifyResult> callback_result,
                            int callback_status) {
                            *result = callback_result.Pass();
                            *status = callback_status;
                            verification_completed = true;
                          });
    base::test::ios::WaitUntilCondition(^{
      return verification_completed;
    });
  }

  // Fake certificate created for testing.
  scoped_refptr<X509Certificate> cert_;
  // Testable |CertVerifierBlockAdapter| object.
  scoped_ptr<CertVerifierBlockAdapter> test_adapter_;
  // CertVerifier mock owned by |test_adapter_|.
  CertVerifierMock* cert_verifier_mock_;
};

// Tests |Verify| with default params and synchronous verification.
TEST_F(CertVerifierBlockAdapterTest, DefaultParamsAndSync) {
  // Set up expectation.
  net::CertVerifyResult expectedResult;
  expectedResult.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  const int kExpectedStatus = 0;
  EXPECT_CALL(*cert_verifier_mock_,
              Verify(cert_.get(), kHostName, "", 0, nullptr, _, _, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(SetVerifyResult(expectedResult),
                               testing::Return(kExpectedStatus)));

  // Call |Verify|.
  scoped_ptr<CertVerifyResult> actualResult;
  int actualStatus = -1;
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  Verify(params, &actualResult, &actualStatus);

  // Ensure that Verification results are correct.
  EXPECT_EQ(kExpectedStatus, actualStatus);
  EXPECT_EQ(expectedResult.cert_status, actualResult->cert_status);
}

// Tests |Verify| with default params and asynchronous verification.
TEST_F(CertVerifierBlockAdapterTest, DefaultParamsAndAsync) {
  // Set up expectation.
  net::CertVerifyResult expectedResult;
  expectedResult.is_issued_by_known_root = true;
  const int kExpectedStatus = 0;
  EXPECT_CALL(*cert_verifier_mock_,
              Verify(cert_.get(), kHostName, "", 0, nullptr, _, _, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(SetVerifyResult(expectedResult), RunCallback(),
                               testing::Return(ERR_IO_PENDING)));

  // Call |Verify|.
  scoped_ptr<CertVerifyResult> actualResult;
  int actualStatus = -1;
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  Verify(params, &actualResult, &actualStatus);

  // Ensure that Verification results are correct.
  EXPECT_EQ(kExpectedStatus, actualStatus);
  EXPECT_EQ(expectedResult.is_issued_by_known_root,
            actualResult->is_issued_by_known_root);
}

// Tests |Verify| with error.
TEST_F(CertVerifierBlockAdapterTest, DefaultParamsAndError) {
  // Set up expectation.
  const int kExpectedStatus = ERR_INSUFFICIENT_RESOURCES;
  EXPECT_CALL(*cert_verifier_mock_,
              Verify(cert_.get(), kHostName, "", 0, nullptr, _, _, _, _))
      .Times(1)
      .WillOnce(testing::Return(kExpectedStatus));

  // Call |Verify|.
  scoped_ptr<CertVerifyResult> actualResult;
  int actualStatus = -1;
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  Verify(params, &actualResult, &actualStatus);

  // Ensure that Verification results are correct.
  EXPECT_EQ(kExpectedStatus, actualStatus);
  EXPECT_FALSE(actualResult);
}

// Tests |Verify| with all params and synchronous verification.
TEST_F(CertVerifierBlockAdapterTest, AllParamsAndSync) {
  // Set up expectation.
  net::CertVerifyResult expectedResult;
  expectedResult.verified_cert = cert_;
  const int kExpectedStatus = 0;
  scoped_refptr<CRLSet> crl_set(CRLSet::EmptyCRLSetForTesting());
  EXPECT_CALL(*cert_verifier_mock_,
              Verify(cert_.get(), kHostName, kOcspResponse,
                     CertVerifier::VERIFY_EV_CERT, crl_set.get(), _, _, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(SetVerifyResult(expectedResult),
                               testing::Return(kExpectedStatus)));

  // Call |Verify|.
  scoped_ptr<CertVerifyResult> actualResult;
  int actualStatus = -1;
  CertVerifierBlockAdapter::Params params(cert_.get(), kHostName);
  params.ocsp_response = kOcspResponse;
  params.flags = CertVerifier::VERIFY_EV_CERT;
  params.crl_set = crl_set;
  Verify(params, &actualResult, &actualStatus);

  // Ensure that Verification results are correct.
  EXPECT_EQ(kExpectedStatus, actualStatus);
  EXPECT_EQ(expectedResult.verified_cert, actualResult->verified_cert);
}

}  // namespace
