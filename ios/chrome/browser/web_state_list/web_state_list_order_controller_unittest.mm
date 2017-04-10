// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/page_transition_types.h"

namespace {
const char kURL[] = "https://chromium.org/";

// A fake NavigationManager used to test opener-opened relationship in the
// WebStateList.
class FakeNavigationManager : public web::TestNavigationManager {
 public:
  FakeNavigationManager() = default;

  // web::NavigationManager implementation.
  int GetLastCommittedItemIndex() const override { return 0; }

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationManager);
};

}  // namespace

class WebStateListOrderControllerTest : public PlatformTest {
 public:
  WebStateListOrderControllerTest()
      : web_state_list_(&web_state_list_delegate_),
        order_controller_(&web_state_list_) {}

 protected:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  WebStateListOrderController order_controller_;

  std::unique_ptr<web::WebState> CreateWebState() {
    auto test_web_state = base::MakeUnique<web::TestWebState>();
    test_web_state->SetCurrentURL(GURL(kURL));
    test_web_state->SetNavigationManager(
        base::MakeUnique<FakeNavigationManager>());
    // TODO(crbug.com/703565): remove std::move() once Xcode 9.0+ is required.
    return std::move(test_web_state);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateListOrderControllerTest);
};

TEST_F(WebStateListOrderControllerTest, DetermineInsertionIndex) {
  web_state_list_.InsertWebState(0, CreateWebState());
  web_state_list_.InsertWebState(1, CreateWebState());
  web::WebState* opener = web_state_list_.GetWebStateAt(0);

  // Verify that first child WebState is inserted after |opener| if there are
  // no other children.
  EXPECT_EQ(1, order_controller_.DetermineInsertionIndex(
                   ui::PAGE_TRANSITION_LINK, opener));

  // Verify that child WebState is inserted at the end if it is not a "LINK"
  // transition.
  EXPECT_EQ(2, order_controller_.DetermineInsertionIndex(
                   ui::PAGE_TRANSITION_GENERATED, opener));

  // Verify that  WebState is inserted at the end if it has no opener.
  EXPECT_EQ(2, order_controller_.DetermineInsertionIndex(
                   ui::PAGE_TRANSITION_LINK, nullptr));

  // Add a child WebState to |opener|, and verify that a second child would be
  // inserted after the first.
  web_state_list_.InsertWebState(2, CreateWebState());
  web_state_list_.SetOpenerOfWebStateAt(2, WebStateOpener(opener));

  EXPECT_EQ(3, order_controller_.DetermineInsertionIndex(
                   ui::PAGE_TRANSITION_LINK, opener));

  // Add a grand-child to |opener|, and verify that adding another child to
  // |opener| would be inserted before the grand-child.
  web_state_list_.InsertWebState(3, CreateWebState());
  web_state_list_.SetOpenerOfWebStateAt(
      3, WebStateOpener(web_state_list_.GetWebStateAt(1)));

  EXPECT_EQ(3, order_controller_.DetermineInsertionIndex(
                   ui::PAGE_TRANSITION_LINK, opener));
}
