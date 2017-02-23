// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list_order_controller.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/page_transition_types.h"

namespace {
const char kURL[] = "https://chromium.org/";

// A fake NavigationManager used to test opener-opened relationship in the
// WebStateList.
class FakeNavigationManer : public web::TestNavigationManager {
 public:
  FakeNavigationManer() = default;

  // web::NavigationManager implementation.
  int GetCurrentItemIndex() const override { return 0; }
  int GetLastCommittedItemIndex() const override { return 0; }

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationManer);
};

}  // namespace

class WebStateListOrderControllerTest : public PlatformTest {
 public:
  WebStateListOrderControllerTest()
      : web_state_list_(WebStateList::WebStateOwned),
        order_controller_(&web_state_list_) {}

 protected:
  WebStateList web_state_list_;
  WebStateListOrderController order_controller_;

  // This method should return std::unique_ptr<> however, due to the complex
  // ownership of Tab, WebStateList currently uses raw pointers. Change this
  // once Tab ownership is sane, see http://crbug.com/546222 for progress.
  web::WebState* CreateWebState() {
    auto test_web_state = base::MakeUnique<web::TestWebState>();
    test_web_state->SetCurrentURL(GURL(kURL));
    test_web_state->SetNavigationManager(
        base::MakeUnique<FakeNavigationManer>());
    return test_web_state.release();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateListOrderControllerTest);
};

TEST_F(WebStateListOrderControllerTest, DetermineInsertionIndex) {
  web_state_list_.InsertWebState(0, CreateWebState(), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(), nullptr);
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
  web_state_list_.InsertWebState(2, CreateWebState(), opener);

  EXPECT_EQ(3, order_controller_.DetermineInsertionIndex(
                   ui::PAGE_TRANSITION_LINK, opener));

  // Add a grand-child to |opener|, and verify that adding another child to
  // |opener| would be inserted before the grand-child.
  web_state_list_.InsertWebState(3, CreateWebState(),
                                 web_state_list_.GetWebStateAt(1));

  EXPECT_EQ(3, order_controller_.DetermineInsertionIndex(
                   ui::PAGE_TRANSITION_LINK, opener));
}
