// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/crw_cert_verification_controller.h"

#include "base/mac/bind_objc_block.h"
#include "base/message_loop/message_loop.h"
#include "base/test/ios/wait_util.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/test/web_test.h"
#include "net/base/test_data_directory.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace web {

namespace {
// Generated cert filename.
const char kCertFileName[] = "ok_cert.pem";
// Test hostname for cert verification.
NSString* const kHostName = @"www.example.com";
}  // namespace

// Test fixture to test CRWCertVerificationController class.
class CRWCertVerificationControllerTest : public web::WebTest {
 protected:
  void SetUp() override {
    web::WebTest::SetUp();

    web::BrowserState* browser_state = GetBrowserState();
    net::URLRequestContextGetter* getter = browser_state->GetRequestContext();
    web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
      getter->GetURLRequestContext()->set_cert_verifier(&cert_verifier_);
    }));

    controller_.reset([[CRWCertVerificationController alloc]
        initWithBrowserState:browser_state]);
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kCertFileName);
  }

  void TearDown() override {
    [controller_ shutDown];
    web::WebTest::TearDown();
  }

  // Synchronously returns result of decidePolicyForCert:host:completionHandler:
  // call.
  void DecidePolicy(const scoped_refptr<net::X509Certificate>& cert,
                    NSString* host,
                    web::CertAcceptPolicy* policy,
                    net::CertStatus* status) {
    __block bool completion_handler_called = false;
    [controller_ decidePolicyForCert:cert
                                host:host
                   completionHandler:^(web::CertAcceptPolicy callback_policy,
                                       net::CertStatus callback_status) {
                     *policy = callback_policy;
                     *status = callback_status;
                     completion_handler_called = true;
                   }];
    base::test::ios::WaitUntilCondition(^{
      return completion_handler_called;
    }, base::MessageLoop::current(), base::TimeDelta());
  }

  scoped_refptr<net::X509Certificate> cert_;
  net::MockCertVerifier cert_verifier_;
  base::scoped_nsobject<CRWCertVerificationController> controller_;
};

// Tests cert policy with a valid cert.
TEST_F(CRWCertVerificationControllerTest, ValidCert) {
  net::CertVerifyResult verify_result;
  verify_result.cert_status = net::CERT_STATUS_NO_REVOCATION_MECHANISM;
  verify_result.verified_cert = cert_;
  cert_verifier_.AddResultForCertAndHost(cert_.get(), [kHostName UTF8String],
                                         verify_result, net::OK);
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(cert_, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_ALLOW, policy);
  EXPECT_EQ(verify_result.cert_status, status);
}

// Tests cert policy with an invalid cert.
TEST_F(CRWCertVerificationControllerTest, InvalidCert) {
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(cert_, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_RECOVERABLE_ERROR, policy);
}

// Tests cert policy with null cert.
TEST_F(CRWCertVerificationControllerTest, NullCert) {
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(nullptr, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR, policy);
}

// Tests cert policy with null cert and null host.
TEST_F(CRWCertVerificationControllerTest, NullHost) {
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(cert_, nil, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR, policy);
}

}  // namespace web
