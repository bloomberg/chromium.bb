// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/wk_web_view_ssl_error_util.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>

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

// Returns an autoreleased certificate chain for testing. Chain will contain a
// single self-signed cert with |subject| as a subject.
NSArray* MakeTestCertChain(const std::string& subject) {
  scoped_ptr<crypto::RSAPrivateKey> private_key;
  std::string der_cert;
  net::x509_util::CreateKeyAndSelfSignedCert(
      "CN=" + subject, 1, base::Time::Now(),
      base::Time::Now() + base::TimeDelta::FromDays(1), &private_key,
      &der_cert);

  SecCertificateRef cert = net::X509Certificate::CreateOSCertHandleFromBytes(
      der_cert.data(), der_cert.size());
  NSArray* result = @[ reinterpret_cast<id>(cert) ];
  CFRelease(cert);
  return result;
}

// Test class for wk_web_view_ssl_error_util functions.
typedef PlatformTest WKWebViewSSLErrorUtilTest;

// Tests that IsWKWebViewSSLError returns true for NSError with NSURLErrorDomain
// domain and NSURLErrorSecureConnectionFailed error code.
TEST_F(WKWebViewSSLErrorUtilTest, CheckSecureConnectionFailedError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_TRUE(web::IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorSecureConnectionFailed
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns true for NSError with NSURLErrorDomain
// domain and NSURLErrorClientCertificateRequired error code.
TEST_F(WKWebViewSSLErrorUtilTest, CheckCannotLoadFromNetworkError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_TRUE(web::IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorClientCertificateRequired
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns false for NSError with empty domain
// and NSURLErrorClientCertificateRequired error code.
TEST_F(WKWebViewSSLErrorUtilTest, CheckCannotLoadFromNetworkErrorWithNoDomain) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_FALSE(web::IsWKWebViewSSLError(
      [NSError errorWithDomain:@""
                          code:NSURLErrorClientCertificateRequired
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns false for NSError with
// NSURLErrorDomain domain and NSURLErrorDataLengthExceedsMaximum error code.
TEST_F(WKWebViewSSLErrorUtilTest, CheckDataLengthExceedsMaximumError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_FALSE(web::IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorDataLengthExceedsMaximum
                      userInfo:nil]));
}

// Tests that IsWKWebViewSSLError returns false for NSError with
// NSURLErrorDomain domain and NSURLErrorCannotLoadFromNetwork error code.
TEST_F(WKWebViewSSLErrorUtilTest, CheckCannotCreateFileError) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  EXPECT_FALSE(web::IsWKWebViewSSLError(
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorCannotLoadFromNetwork
                      userInfo:nil]));
}

// Tests GetSSLInfoFromWKWebViewSSLError with NSError without user info.
TEST_F(WKWebViewSSLErrorUtilTest, SSLInfoFromErrorWithoutUserInfo) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  NSError* badDateError =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasBadDate
                      userInfo:nil];
  net::SSLInfo info;
  web::GetSSLInfoFromWKWebViewSSLError(badDateError, &info);
  EXPECT_TRUE(info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_DATE_INVALID, info.cert_status);
  info.cert->subject();
  EXPECT_EQ("", info.cert->subject().GetDisplayName());
}

// Tests GetSSLInfoFromWKWebViewSSLError with NSError without cert.
TEST_F(WKWebViewSSLErrorUtilTest, SSLInfoFromErrorWithoutCert) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  NSError* untrustedCertError = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorServerCertificateUntrusted
             userInfo:@{
               NSURLErrorFailingURLStringErrorKey : @"https://www.google.com/",
             }];
  net::SSLInfo info;
  web::GetSSLInfoFromWKWebViewSSLError(untrustedCertError, &info);
  EXPECT_TRUE(info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID, info.cert_status);
  EXPECT_EQ("https://www.google.com/", info.cert->subject().GetDisplayName());
}

// Tests GetSSLInfoFromWKWebViewSSLError with NSError and self-signed cert.
TEST_F(WKWebViewSSLErrorUtilTest, SSLInfoFromErrorWithCert) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  const std::string kTestSubject = "self-signed";
  NSError* unknownCertError =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:@{
                        web::kNSErrorPeerCertificateChainKey :
                                 MakeTestCertChain(kTestSubject),
                      }];

  net::SSLInfo info;
  web::GetSSLInfoFromWKWebViewSSLError(unknownCertError, &info);
  EXPECT_TRUE(info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID, info.cert_status);
  EXPECT_EQ(kTestSubject, info.cert->subject().GetDisplayName());
}

}  // namespace
}  // namespace web
