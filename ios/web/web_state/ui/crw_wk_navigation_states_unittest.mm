// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_navigation_states.h"

#import <WebKit/WebKit.h>

#import "base/mac/scoped_nsobject.h"
#include "ios/web/web_state/navigation_context_impl.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {
const char kTestUrl1[] = "https://test1.test/";
const char kTestUrl2[] = "https://test2.test/";
}

namespace web {

// Test fixture for CRWWKNavigationStates testing.
class CRWWKNavigationStatesTest : public PlatformTest {
 protected:
  CRWWKNavigationStatesTest()
      : navigation1_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        navigation2_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        navigation3_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        states_([[CRWWKNavigationStates alloc] init]) {}

 protected:
  base::scoped_nsobject<WKNavigation> navigation1_;
  base::scoped_nsobject<WKNavigation> navigation2_;
  base::scoped_nsobject<WKNavigation> navigation3_;
  base::scoped_nsobject<CRWWKNavigationStates> states_;
};

// Tests |removeNavigation:| method.
TEST_F(CRWWKNavigationStatesTest, RemovingNavigation) {
  // navigation_1 is the only navigation and it is the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  ASSERT_EQ(navigation1_, [states_ lastAddedNavigation]);
  [states_ removeNavigation:navigation1_];
  ASSERT_FALSE([states_ lastAddedNavigation]);
}

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

  // navigation_3 is added later and hence the latest.
  std::unique_ptr<web::NavigationContextImpl> context =
      NavigationContextImpl::CreateSameDocumentNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl1));
  [states_ setContext:std::move(context) forNavigation:navigation3_];
  EXPECT_EQ(navigation3_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::NONE, [states_ lastAddedNavigationState]);
}

// Tests |setContext:forNavigation:| and |contextForNavigaiton:| methods.
TEST_F(CRWWKNavigationStatesTest, Context) {
  EXPECT_FALSE([states_ contextForNavigation:navigation1_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation2_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation3_]);

  // Add first context.
  std::unique_ptr<web::NavigationContextImpl> context1 =
      NavigationContextImpl::CreateSameDocumentNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl1));
  [states_ setContext:std::move(context1) forNavigation:navigation1_];
  EXPECT_FALSE([states_ contextForNavigation:navigation2_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation3_]);
  ASSERT_TRUE([states_ contextForNavigation:navigation1_]);
  EXPECT_EQ(GURL(kTestUrl1),
            [states_ contextForNavigation:navigation1_]->GetUrl());
  EXPECT_TRUE([states_ contextForNavigation:navigation1_]->IsSameDocument());
  EXPECT_FALSE([states_ contextForNavigation:navigation1_]->IsErrorPage());

  // Replace existing context.
  std::unique_ptr<web::NavigationContextImpl> context2 =
      NavigationContextImpl::CreateErrorPageNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl2),
          nullptr /* response_headers */);
  [states_ setContext:std::move(context2) forNavigation:navigation1_];
  EXPECT_FALSE([states_ contextForNavigation:navigation2_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation3_]);
  ASSERT_TRUE([states_ contextForNavigation:navigation1_]);
  EXPECT_EQ(GURL(kTestUrl2),
            [states_ contextForNavigation:navigation1_]->GetUrl());
  EXPECT_FALSE([states_ contextForNavigation:navigation1_]->IsSameDocument());
  EXPECT_TRUE([states_ contextForNavigation:navigation1_]->IsErrorPage());
}

// Tests null WKNavigation object.
TEST_F(CRWWKNavigationStatesTest, NullNavigation) {
  // navigation_1 is the only navigation and it is the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  ASSERT_EQ(navigation1_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // null navigation is added later and hence the latest.
  [states_ setState:WKNavigationState::STARTED forNavigation:nil];
  EXPECT_FALSE([states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::STARTED, [states_ lastAddedNavigationState]);

  // navigation_1 is the is the latest again after removing null navigation.
  [states_ removeNavigation:nil];
  ASSERT_EQ(navigation1_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);
}

}  // namespace web
