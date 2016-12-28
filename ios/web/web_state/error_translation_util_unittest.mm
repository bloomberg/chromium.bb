// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/error_translation_util.h"

#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

// Test fixture for error translation testing.
typedef PlatformTest ErrorTranslationUtilTest;

namespace {
// Returns net error domain.
NSString* GetNetErrorDomain() {
  return base::SysUTF8ToNSString(net::kErrorDomain);
}
}  // namespcae

// Tests translation of an error with empty domain and no underlying error.
TEST_F(ErrorTranslationUtilTest, MalformedError) {
  base::scoped_nsobject<NSError> error(
      [[NSError alloc] initWithDomain:@"" code:0 userInfo:nil]);
  NSError* net_error = web::NetErrorFromError(error);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // Underlying error should have net error doamin and code.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(net_underlying_error);
  EXPECT_NSEQ(GetNetErrorDomain(), [net_underlying_error domain]);
  EXPECT_EQ(net::ERR_FAILED, [net_underlying_error code]);
}

// Tests translation of unknown CFNetwork error, which does not have an
// underlying error.
TEST_F(ErrorTranslationUtilTest, UnknownCFNetworkError) {
  base::scoped_nsobject<NSError> error([[NSError alloc]
      initWithDomain:static_cast<NSString*>(kCFErrorDomainCFNetwork)
                code:kCFURLErrorUnknown
            userInfo:nil]);
  NSError* net_error = web::NetErrorFromError(error);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // Underlying error should have net error domain and code.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(net_underlying_error);
  EXPECT_NSEQ(GetNetErrorDomain(), [net_underlying_error domain]);
  EXPECT_EQ(net::ERR_FAILED, [net_underlying_error code]);
}

// Tests translation of kCFURLErrorCannotFindHost CFNetwork error, which has an
// underlying error with NSURLError domain.
TEST_F(ErrorTranslationUtilTest, CanNotFindHostError) {
  base::scoped_nsobject<NSError> underlying_error([[NSError alloc]
      initWithDomain:NSURLErrorDomain
                code:kCFURLErrorCannotFindHost
            userInfo:nil]);

  NSError* error =
      [[NSError alloc] initWithDomain:NSURLErrorDomain
                                 code:NSURLErrorCannotFindHost
                             userInfo:@{
                               NSUnderlyingErrorKey : underlying_error,
                             }];
  NSError* net_error = web::NetErrorFromError(error);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // First underlying error should be the same as the original underlying error.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(underlying_error);
  EXPECT_NSEQ([underlying_error domain], [net_underlying_error domain]);
  EXPECT_EQ([underlying_error code], [net_underlying_error code]);

  // Final underlying error should have net error domain and code.
  NSError* final_net_underlying_error =
      [net_underlying_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(final_net_underlying_error);
  EXPECT_NSEQ(GetNetErrorDomain(), [final_net_underlying_error domain]);
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, [final_net_underlying_error code]);
}

// Tests translation of kCFURLErrorSecureConnectionFailed CFNetwork error, by
// specifying different net error code.
TEST_F(ErrorTranslationUtilTest, CertError) {
  base::scoped_nsobject<NSError> underlying_error([[NSError alloc]
      initWithDomain:NSURLErrorDomain
                code:kCFURLErrorSecureConnectionFailed
            userInfo:nil]);

  NSError* error =
      [[NSError alloc] initWithDomain:NSURLErrorDomain
                                 code:kCFURLErrorSecureConnectionFailed
                             userInfo:@{
                               NSUnderlyingErrorKey : underlying_error,
                             }];
  NSError* net_error = web::NetErrorFromError(error, net::ERR_CONNECTION_RESET);

  // Top level error should be the same as the original error.
  EXPECT_TRUE(net_error);
  EXPECT_NSEQ([error domain], [net_error domain]);
  EXPECT_EQ([error code], [net_error code]);

  // First underlying error should be the same as the original underlying error.
  NSError* net_underlying_error = [net_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(underlying_error);
  EXPECT_NSEQ([underlying_error domain], [net_underlying_error domain]);
  EXPECT_EQ([underlying_error code], [net_underlying_error code]);

  // Final underlying error should have net error domain and specified code.
  NSError* final_net_underlying_error =
      [net_underlying_error userInfo][NSUnderlyingErrorKey];
  EXPECT_TRUE(final_net_underlying_error);
  EXPECT_NSEQ(GetNetErrorDomain(), [final_net_underlying_error domain]);
  EXPECT_EQ(net::ERR_CONNECTION_RESET, [final_net_underlying_error code]);
}
