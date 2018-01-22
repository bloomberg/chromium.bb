// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/reauthentication_module_for_testing.h"

#import <LocalAuthentication/LocalAuthentication.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestingSuccessfulReauthTimeAccessor
    : NSObject<SuccessfulReauthTimeAccessor> {
  // Object storing the time of a fake previous successful re-authentication
  // to be used by the |ReauthenticationModule|.
  NSDate* successfulReauthTime_;
}

@end

@implementation TestingSuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  successfulReauthTime_ = [[NSDate alloc] init];
}

- (void)updateSuccessfulReauthTime:(NSDate*)time {
  successfulReauthTime_ = time;
}

- (NSDate*)lastSuccessfulReauthTime {
  return successfulReauthTime_;
}

@end

namespace {

class ReauthenticationModuleTest : public ::testing::Test {
 protected:
  ReauthenticationModuleTest() {}

  void SetUp() override {
    auth_context_ = [OCMockObject niceMockForClass:[LAContext class]];
    time_accessor_ = [[TestingSuccessfulReauthTimeAccessor alloc] init];
    reauthentication_module_ = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:time_accessor_];
    [reauthentication_module_ setCreateLAContext:^LAContext*() {
      return auth_context_;
    }];
  }

  id auth_context_;
  TestingSuccessfulReauthTimeAccessor* time_accessor_;
  ReauthenticationModule* reauthentication_module_;
};

TEST_F(ReauthenticationModuleTest, ReauthReuseNotPermitted) {
  [time_accessor_ updateSuccessfulReauthTime];

  OCMExpect([auth_context_ evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                          localizedReason:[OCMArg any]
                                    reply:[OCMArg any]]);
  [reauthentication_module_ attemptReauthWithLocalizedReason:@"Test"
                                        canReusePreviousAuth:NO
                                                     handler:^(BOOL success){
                                                     }];

  EXPECT_OCMOCK_VERIFY(auth_context_);
}

TEST_F(ReauthenticationModuleTest, ReauthReusePermittedLessThanSixtySeconds) {
  [time_accessor_ updateSuccessfulReauthTime];

  [[auth_context_ reject] evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                         localizedReason:[OCMArg any]
                                   reply:[OCMArg any]];

  // Use @try/@catch as -reject raises an exception.
  @try {
    [reauthentication_module_ attemptReauthWithLocalizedReason:@"Test"
                                          canReusePreviousAuth:YES
                                                       handler:^(BOOL success){
                                                       }];
    EXPECT_OCMOCK_VERIFY(auth_context_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - attemptReauthWithLocalizedReason:canReusePreviousAuth:handler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
}

TEST_F(ReauthenticationModuleTest, ReauthReusePermittedMoreThanSixtySeconds) {
  const int kIntervalForFakePreviousAuthInSeconds = -70;
  [time_accessor_ updateSuccessfulReauthTime:
                      [NSDate dateWithTimeIntervalSinceNow:
                                  kIntervalForFakePreviousAuthInSeconds]];

  OCMExpect([auth_context_ evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                          localizedReason:[OCMArg any]
                                    reply:[OCMArg any]]);
  [reauthentication_module_ attemptReauthWithLocalizedReason:@"Test"
                                        canReusePreviousAuth:YES
                                                     handler:^(BOOL success){
                                                     }];

  EXPECT_OCMOCK_VERIFY(auth_context_);
}

}  // namespace
