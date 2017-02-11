// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_navigation_states.h"

#import <WebKit/WebKit.h>

#import "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

// Test fixture for CRWWKNavigationStates testing.
class CRWWKNavigationStatesTest : public PlatformTest {
 protected:
  CRWWKNavigationStatesTest()
      : navigation1_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        navigation2_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        states_([[CRWWKNavigationStates alloc] init]) {}

 protected:
  base::scoped_nsobject<WKNavigation> navigation1_;
  base::scoped_nsobject<WKNavigation> navigation2_;
  base::scoped_nsobject<CRWWKNavigationStates> states_;
};

// Tests |lastAddedNavigation| method.
TEST_F(CRWWKNavigationStatesTest, LastAddedNavigation) {
  // navigation_1 is the only navigation and it is the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  EXPECT_EQ(navigation1_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // navigation_2 is added later and hence the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation2_];
  EXPECT_EQ(navigation2_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // Updating state for existing navigation does not make it the latest.
  [states_ setState:WKNavigationState::STARTED forNavigation:navigation1_];
  EXPECT_EQ(navigation2_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // navigation_2 is still the latest.
  [states_ setState:WKNavigationState::STARTED forNavigation:navigation2_];
  EXPECT_EQ(navigation2_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::STARTED, [states_ lastAddedNavigationState]);
}

}  // namespace web
