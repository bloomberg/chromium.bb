// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_placeholder_navigation_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for CRWPlaceholderNavigationInfo.
typedef PlatformTest CRWPlaceholderNavigationInfoTest;

// Tests that completion handler is saved and retrieved from a navigation.
TEST_F(CRWPlaceholderNavigationInfoTest, SetInfoForNavigation) {
  WKNavigation* navigation = OCMClassMock([WKNavigation class]);
  EXPECT_NSEQ(nil, [CRWPlaceholderNavigationInfo infoForNavigation:navigation]);

  __block BOOL called = NO;
  ProceduralBlock completionHandler = ^{
    called = YES;
  };

  CRWPlaceholderNavigationInfo* info =
      [CRWPlaceholderNavigationInfo createForNavigation:navigation
                                  withCompletionHandler:completionHandler];
  EXPECT_EQ(NO, called);

  CRWPlaceholderNavigationInfo* retrievedInfo =
      [CRWPlaceholderNavigationInfo infoForNavigation:navigation];
  ASSERT_NSEQ(info, retrievedInfo);
  EXPECT_EQ(NO, called);

  [retrievedInfo runCompletionHandler];
  EXPECT_EQ(YES, called);
}

TEST_F(CRWPlaceholderNavigationInfoTest, GetInfoOnNilNavigationReturnsNil) {
  EXPECT_NSEQ(nil, [CRWPlaceholderNavigationInfo infoForNavigation:nil]);
}
