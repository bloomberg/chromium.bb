// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/app_nap_activity.h"

#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

class AppNapActivityTest : public PlatformTest {};

TEST_F(AppNapActivityTest, StoresAssertion) {
  const NSActivityOptions expectedOptions =
      (NSActivityUserInitiatedAllowingIdleSystemSleep |
       NSActivityLatencyCritical) &
      ~(NSActivitySuddenTerminationDisabled |
        NSActivityAutomaticTerminationDisabled);
  id processInfoMock =
      [OCMockObject partialMockForObject:[NSProcessInfo processInfo]];
  id assertion = @"An activity assertion";
  [[[processInfoMock expect] andReturn:assertion]
      beginActivityWithOptions:expectedOptions
                        reason:OCMOCK_ANY];

  content::AppNapActivity activity;
  activity.Begin();

  EXPECT_OCMOCK_VERIFY(processInfoMock);

  [[processInfoMock expect] endActivity:assertion];

  activity.End();

  EXPECT_OCMOCK_VERIFY(processInfoMock);
  [processInfoMock stopMocking];
}
