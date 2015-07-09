// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/wk_web_view_security_util.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/rsa_private_key.h"
#include "ios/web/public/test/web_test_util.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {
// Subject for testing self-signed certificate.
const char kTestSubject[] = "self-signed";

// Returns an autoreleased certificate chain for testing. Chain will contain a
// single self-signed cert with |subject| as a subject.
NSArray* MakeTestCertChain(const std::string& subject) {
  scoped_ptr<crypto::RSAPrivateKey> private_key;
  std::string der_cert;
  net::x509_util::CreateKeyAndSelfSignedCert(
      "CN=" + subject, 1, base::Time::Now(),
      base::Time::Now() + base::TimeDelta::FromDays(1), &private_key,
      &der_cert);

  base::ScopedCFTypeRef<SecCertificateRef> cert(
      net::X509Certificate::CreateOSCertHandleFromBytes(der_cert.data(),
                                                        der_cert.size()));
  NSArray* result = @[ reinterpret_cast<id>(cert.get()) ];
  return result;
}

// Returns SecTrustRef object for testing.
base::ScopedCFTypeRef<SecTrustRef> CreateTestTrust(NSArray* cert_chain) {
  base::ScopedCFTypeRef<SecPolicyRef> policy(SecPolicyCreateBasicX509());
  SecTrustRef trust = nullptr;
  SecTrustCreateWithCertificates(cert_chain, policy, &trust);
  return base::ScopedCFTypeRef<SecTrustRef>(trust);
}

}  // namespace

// Test class for wk_web_view_security_util functions.
typedef PlatformTest WKWebViewSecurityUtilTest;

// Tests CreateCertFromChain with self-signed cert.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromChain) {
  scoped_refptr<net::X509Certificate> cert =
      CreateCertFromChain(MakeTestCertChain(kTestSubject));
  EXPECT_TRUE(cert->subject().GetDisplayName() == kTestSubject);
}

// Tests CreateCertFromChain with nil chain.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromNilChain) {
  EXPECT_FALSE(CreateCertFromChain(nil));
}

// Tests CreateCertFromChain with empty chain.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromEmptyChain) {
  EXPECT_FALSE(CreateCertFromChain(@[]));
}

// Tests MakeTrustValid with self-signed cert.
TEST_F(WKWebViewSecurityUtilTest, MakingTrustValid) {
  // Create invalid trust object.
  base::ScopedCFTypeRef<SecTrustRef> trust =
      CreateTestTrust(MakeTestCertChain(kTestSubject));

  SecTrustResultType result = -1;
  SecTrustEvaluate(trust, &result);
  EXPECT_EQ(kSecTrustResultRecoverableTrustFailure, result);

  // Make sure that trust becomes valid after
  // |EnsureFutureTrustEvaluationSucceeds| call.
  EnsureFutureTrustEvaluationSucceeds(trust);
  SecTrustEvaluate(trust, &result);
  EXPECT_EQ(kSecTrustResultProceed, result);
}

// Tests CreateCertFromTrust.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromTrust) {
  base::ScopedCFTypeRef<SecTrustRef> trust =
      CreateTestTrust(MakeTestCertChain(kTestSubject));
  scoped_refptr<net::X509Certificate> cert = CreateCertFromTrust(trust);
  EXPECT_TRUE(cert->subject().GetDisplayName() == kTestSubject);
}

// Tests CreateCertFromTrust with nil trust.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromNilTrust) {
  EXPECT_FALSE(CreateCertFromTrust(nil));
}

// Tests that IsWKWebViewSSLError returns true for NSError with NSURLErrorDomain
// domain and NSURLErrorSecureConnectionFailed error code.
TEST_F(WKWebViewSecurityUtilTest, CheckSecureConnectionFailedError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_TRUE(IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorSecureConnectionFailed
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns true for NSError with NSURLErrorDomain
// domain and NSURLErrorClientCertificateRequired error code.
TEST_F(WKWebViewSecurityUtilTest, CheckCannotLoadFromNetworkError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_TRUE(IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorClientCertificateRequired
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns false for NSError with empty domain
// and NSURLErrorClientCertificateRequired error code.
TEST_F(WKWebViewSecurityUtilTest, CheckCannotLoadFromNetworkErrorWithNoDomain) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_FALSE(IsWKWebViewSSLError(
      [NSError errorWithDomain:@""
                          code:NSURLErrorClientCertificateRequired
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns false for NSError with
// NSURLErrorDomain domain and NSURLErrorDataLengthExceedsMaximum error code.
TEST_F(WKWebViewSecurityUtilTest, CheckDataLengthExceedsMaximumError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_FALSE(IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorDataLengthExceedsMaximum
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns false for NSError with
// NSURLErrorDomain domain and NSURLErrorCannotLoadFromNetwork error code.
TEST_F(WKWebViewSecurityUtilTest, CheckCannotCreateFileError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_FALSE(IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorCannotLoadFromNetwork
                      userInfo:nil]));
}

// Tests GetSSLInfoFromWKWebViewSSLError with NSError without user info.
TEST_F(WKWebViewSecurityUtilTest, SSLInfoFromErrorWithoutUserInfo) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  NSError* badDateError =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasBadDate
                      userInfo:nil];
  net::SSLInfo info;
  GetSSLInfoFromWKWebViewSSLError(badDateError, &info);
  EXPECT_TRUE(info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID, info.cert_status);
  info.cert->subject();
  EXPECT_EQ("", info.cert->subject().GetDisplayName());
}

// Tests GetSSLInfoFromWKWebViewSSLError with NSError without cert.
TEST_F(WKWebViewSecurityUtilTest, SSLInfoFromErrorWithoutCert) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  NSError* untrustedCertError = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorServerCertificateUntrusted
             userInfo:@{
               NSURLErrorFailingURLStringErrorKey : @"https://www.google.com/",
             }];
  net::SSLInfo info;
  GetSSLInfoFromWKWebViewSSLError(untrustedCertError, &info);
  EXPECT_TRUE(info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID, info.cert_status);
  EXPECT_EQ("https://www.google.com/", info.cert->subject().GetDisplayName());
}

// Tests GetSSLInfoFromWKWebViewSSLError with NSError and self-signed cert.
TEST_F(WKWebViewSecurityUtilTest, SSLInfoFromErrorWithCert) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  NSError* unknownCertError =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:@{
                        kNSErrorPeerCertificateChainKey :
                            MakeTestCertChain(kTestSubject),
                      }];

  net::SSLInfo info;
  GetSSLInfoFromWKWebViewSSLError(unknownCertError, &info);
  EXPECT_TRUE(info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID, info.cert_status);
  EXPECT_TRUE(info.cert->subject().GetDisplayName() == kTestSubject);
}

}  // namespace web
