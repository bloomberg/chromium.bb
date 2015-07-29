// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/crw_cert_policy_cache.h"

#import "base/mac/scoped_nsobject.h"
#import "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace net {
namespace {

// Test hostname for CertVerifier.
const char kHostName[] = "chromium.org";

// Mocks CertificatePolicyCache for CRWCertPolicyCache testing.
class CertificatePolicyCacheMock : public web::CertificatePolicyCache {
 public:
  MOCK_METHOD3(AllowCertForHost,
               void(X509Certificate *cert, const std::string &host,
                    net::CertStatus error));
  MOCK_METHOD3(QueryPolicy, web::CertPolicy::Judgment(X509Certificate *cert,
                                                      const std::string &host,
                                                      net::CertStatus error));

private:
  ~CertificatePolicyCacheMock() {}  // RefCounted requirement.
};

// Verfies that current thread is IOThread.
ACTION(CheckIOThread) {
  DCHECK(web::WebThread::CurrentlyOn(web::WebThread::IO));
}

}  // namespace

// Test fixture to test CRWCertPolicyCache class.
class CRWCertPolicyCacheTest : public PlatformTest {
 protected:
  CRWCertPolicyCacheTest()
      : cert_(new X509Certificate("test", "test", base::Time(), base::Time())),
        cache_mock_(new CertificatePolicyCacheMock()),
        testable_([[CRWCertPolicyCache alloc] initWithCache:cache_mock_]),
        thread_bundle_(web::TestWebThreadBundle::REAL_IO_THREAD) {}

  // Fake certificate created for testing.
  scoped_refptr<X509Certificate> cert_;
  // CertificatePolicyCacheMock.
  scoped_refptr<CertificatePolicyCacheMock> cache_mock_;
  // Testable |CertVerifierBlockAdapter| object.
  base::scoped_nsobject<CRWCertPolicyCache> testable_;
  // The threads used for testing.
  web::TestWebThreadBundle thread_bundle_;
};

// Tests |allowCert:forHost:status:| with default arguments.
TEST_F(CRWCertPolicyCacheTest, TestAllowingCertForHost) {
  // Set up expectation.
  EXPECT_CALL(*cache_mock_,
              AllowCertForHost(cert_.get(), kHostName, CERT_STATUS_ALL_ERRORS))
      .Times(1)
      .WillOnce(CheckIOThread());

  // Run and wait for completion.
  __block bool verification_completed = false;
  [testable_ allowCert:cert_.get()
                forHost:base::SysUTF8ToNSString(kHostName)
                 status:CERT_STATUS_ALL_ERRORS
      completionHandler:^{
        verification_completed = true;
      }];

  base::test::ios::WaitUntilCondition(^{
    return verification_completed;
  });
}

// Tests |queryJudgementForCert:forHost:status:| with default arguments.
TEST_F(CRWCertPolicyCacheTest, TestQueryJudgment) {
  // Set up expectation.
  web::CertPolicy::Judgment expected_policy = web::CertPolicy::ALLOWED;
  EXPECT_CALL(*cache_mock_,
              QueryPolicy(cert_.get(), kHostName, CERT_STATUS_ALL_ERRORS))
      .Times(1)
      .WillOnce(
          testing::DoAll(CheckIOThread(), testing::Return(expected_policy)));

  // Run and wait for completion.
  __block bool verification_completed = false;
  [testable_ queryJudgementForCert:cert_.get()
                           forHost:base::SysUTF8ToNSString(kHostName)
                            status:CERT_STATUS_ALL_ERRORS
                 completionHandler:^(web::CertPolicy::Judgment policy) {
                   DCHECK_EQ(expected_policy, policy);
                   verification_completed = true;
                 }];

  base::test::ios::WaitUntilCondition(^{
    return verification_completed;
  });
}

}  // namespace net
