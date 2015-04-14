// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/alloc_with_zone_interceptor.h"

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

// Testable class to test web::AddAllocWithZoneMethod.
@interface CRBTestAlloc : NSObject
@end
@implementation CRBTestAlloc
@end

namespace web {
namespace {

// Test fixture for alloc_with_zone_interceptor functions.
typedef PlatformTest AllocWithZoneInterceptorTest;

// Tests that AddAllocWithZoneMethod delegates allocation to the caller.
TEST_F(AllocWithZoneInterceptorTest, AddAllocWithZoneMethod) {
  NSZone* const kZone = NSDefaultMallocZone();
  base::scoped_nsobject<CRBTestAlloc> original_result(
      [CRBTestAlloc allocWithZone:kZone]);
  ASSERT_TRUE([original_result isMemberOfClass:[CRBTestAlloc class]]);
  NSString* const kResult = @"test-result";
  AddAllocWithZoneMethod([CRBTestAlloc class], ^id(Class klass, NSZone* zone) {
    EXPECT_EQ([CRBTestAlloc class], klass);
    EXPECT_EQ(kZone, zone);
    return kResult;
  });
  EXPECT_NSEQ(kResult, [CRBTestAlloc allocWithZone:kZone]);
}

}  // namespace
}  // namespace web
