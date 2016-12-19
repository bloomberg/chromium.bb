// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"

#import "breakpad/src/client/ios/BreakpadController.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@implementation OCMockObject (BreakpadControllerTesting)

- (void)cr_expectGetCrashReportCount:(int)crashReportCount {
  id invocationBlock = ^(NSInvocation* invocation) {
    void (^block)(int);
    [invocation getArgument:&block atIndex:2];
    if (!block) {
      ADD_FAILURE();
      return;
    }
    block(crashReportCount);
  };
  BreakpadController* breakpadController =
      static_cast<BreakpadController*>([[self expect] andDo:invocationBlock]);
  [breakpadController getCrashReportCount:[OCMArg isNotNil]];
}

@end
