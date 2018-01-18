// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_opener.h"

#include <memory>

#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class FakeNavigationManager : public web::TestNavigationManager {
 public:
  explicit FakeNavigationManager(int last_committed_item_index)
      : last_committed_item_index_(last_committed_item_index) {}

  // web::NavigationManager implementation.
  int GetLastCommittedItemIndex() const override {
    return last_committed_item_index_;
  }

 private:
  int last_committed_item_index_;

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationManager);
};

}  // namespace

class WebStateOpenerTest : public PlatformTest {
 public:
  WebStateOpenerTest() = default;

  std::unique_ptr<web::WebState> CreateWebState(int last_committed_item_index) {
    auto test_web_state = std::make_unique<web::TestWebState>();
    test_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>(last_committed_item_index));
    return test_web_state;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateOpenerTest);
};

TEST_F(WebStateOpenerTest, NullWebState) {
  WebStateOpener opener(nullptr);

  EXPECT_EQ(nullptr, opener.opener);
  EXPECT_EQ(-1, opener.navigation_index);
}

TEST_F(WebStateOpenerTest, DefaultNavigationIndex) {
  std::unique_ptr<web::WebState> web_state = CreateWebState(2);
  WebStateOpener opener(web_state.get());

  EXPECT_EQ(web_state.get(), opener.opener);
  EXPECT_EQ(2, opener.navigation_index);
}

TEST_F(WebStateOpenerTest, ExplicitNavigationIndex) {
  std::unique_ptr<web::WebState> web_state = CreateWebState(2);
  WebStateOpener opener(web_state.get(), 1);

  EXPECT_EQ(web_state.get(), opener.opener);
  EXPECT_EQ(1, opener.navigation_index);
}
