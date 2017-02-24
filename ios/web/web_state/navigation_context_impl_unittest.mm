// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/navigation_context_impl.h"

#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

// Test fixture for NavigationContextImplTest testing.
class NavigationContextImplTest : public PlatformTest {
 protected:
  NavigationContextImplTest() : url_("https://chromium.test") {}

  TestWebState web_state_;
  GURL url_;
};

// Tests CreateNavigationContext factory method.
TEST_F(NavigationContextImplTest, NavigationContext) {
  std::unique_ptr<NavigationContext> context =
      NavigationContextImpl::CreateNavigationContext(&web_state_, url_);
  ASSERT_TRUE(context);

  EXPECT_EQ(&web_state_, context->GetWebState());
  EXPECT_EQ(url_, context->GetUrl());
  EXPECT_FALSE(context->IsSamePage());
  EXPECT_FALSE(context->IsErrorPage());
}

// Tests CreateSamePageNavigationContext factory method.
TEST_F(NavigationContextImplTest, SamePageNavigationContext) {
  std::unique_ptr<NavigationContext> context =
      NavigationContextImpl::CreateSamePageNavigationContext(&web_state_, url_);
  ASSERT_TRUE(context);

  EXPECT_EQ(&web_state_, context->GetWebState());
  EXPECT_EQ(url_, context->GetUrl());
  EXPECT_TRUE(context->IsSamePage());
  EXPECT_FALSE(context->IsErrorPage());
}

// Tests CreateErrorPageNavigationContext factory method.
TEST_F(NavigationContextImplTest, ErrorPageNavigationContext) {
  std::unique_ptr<NavigationContext> context =
      NavigationContextImpl::CreateErrorPageNavigationContext(&web_state_,
                                                              url_);
  ASSERT_TRUE(context);

  EXPECT_EQ(&web_state_, context->GetWebState());
  EXPECT_EQ(url_, context->GetUrl());
  EXPECT_FALSE(context->IsSamePage());
  EXPECT_TRUE(context->IsErrorPage());
}

}  // namespace web
