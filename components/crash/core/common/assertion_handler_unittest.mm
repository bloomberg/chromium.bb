// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/crash/core/common/assertion_handler.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

@interface TestNSAssert : NSObject
- (void)assert;
@end

@implementation TestNSAssert
- (void)assert {
  NSAssert1(NO, @"Test for NSAssert is %@", @"fatal");
}
@end

namespace {

void DoTestNSAssert() {
  [[[[TestNSAssert alloc] init] autorelease] assert];
}

void TestNSCAssert() {
  NSCAssert1(NO, @"Test for NSAssert is %@", @"fatal");
}

using ParamType = void (*)(void);

class AssertionHandlerTest : public testing::TestWithParam<ParamType> {
 public:
  void SetUp() override {
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    Reset();
  }

  void TearDown() override { Reset(); }

  void Reset() {
    NSMutableDictionary* threadDictionary =
        [[NSThread currentThread] threadDictionary];
    [threadDictionary removeObjectForKey:NSAssertionHandlerKey];
  }
};

TEST_P(AssertionHandlerTest, DefaultBehavior) {
  bool did_catch = false;
  @try {
    GetParam()();
  } @catch (id exc) {
    EXPECT_NSEQ(NSInternalInconsistencyException, [exc name]);
    EXPECT_NSEQ(@"Test for NSAssert is fatal", [exc reason]);
    did_catch = true;
  }
  EXPECT_TRUE(did_catch);
}

TEST_P(AssertionHandlerTest, CustomHandler) {
  auto death_test = [](ParamType test_param) {
    crash_reporter::InstallNSAssertionHandlerOnCurrentThread();
    test_param();
  };

  @try {
    EXPECT_DEATH_IF_SUPPORTED(
        death_test(GetParam()),
        ".*FATAL:assertion_handler\\.mm.*Test for NSAssert is fatal");
  } @catch (id exc) {
    ADD_FAILURE() << "Assertion failures should be fatal";
  }
}

TEST_P(AssertionHandlerTest, ReplaceHandler) {
  auto death_test = [](ParamType test_param) {
    // Initialize the default NSAssertionHandler.
    CHECK([NSAssertionHandler currentHandler]);
    // Replace it with the custom one.
    crash_reporter::InstallNSAssertionHandlerOnCurrentThread();
    test_param();
  };

  @try {
    EXPECT_DEATH_IF_SUPPORTED(
        death_test(GetParam()),
        ".*FATAL:assertion_handler\\.mm.*Test for NSAssert is fatal");
  } @catch (id exc) {
    ADD_FAILURE() << "Assertion failures should be fatal";
  }
}

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        AssertionHandlerTest,
                        testing::Values(&DoTestNSAssert, &TestNSCAssert));

}  // namespace
