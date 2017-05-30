// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_manager_util.h"

#include "base/memory/ptr_util.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Test fixture testing navigation_manager_util.h functions.
class NavigationManagerUtilTest : public PlatformTest {
 protected:
  NavigationManagerUtilTest()
      : controller_([[CRWSessionController alloc]
            initWithBrowserState:&browser_state_]) {
    manager_.SetSessionController(controller_);
  }

  NavigationManagerImpl manager_;
  CRWSessionController* controller_;

 private:
  TestBrowserState browser_state_;
};

// Tests GetCommittedItemWithUniqueID, GetCommittedItemIndexWithUniqueID and
// GetItemWithUniqueID functions.
TEST_F(NavigationManagerUtilTest, GetCommittedItemWithUniqueID) {
  // Start with NavigationManager that only has a pending item.
  manager_.AddPendingItem(
      GURL("http://chromium.org"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  NavigationItem* item = manager_.GetPendingItem();
  int unique_id = item->GetUniqueID();
  EXPECT_FALSE(GetCommittedItemWithUniqueID(&manager_, unique_id));
  EXPECT_EQ(item, GetItemWithUniqueID(&manager_, unique_id));
  EXPECT_EQ(-1, GetCommittedItemIndexWithUniqueID(&manager_, unique_id));

  // Commit that pending item.
  [controller_ commitPendingItem];
  EXPECT_EQ(item, GetCommittedItemWithUniqueID(&manager_, unique_id));
  EXPECT_EQ(item, GetItemWithUniqueID(&manager_, unique_id));
  EXPECT_EQ(0, GetCommittedItemIndexWithUniqueID(&manager_, unique_id));

  // Remove committed item.
  manager_.RemoveItemAtIndex(0);
  EXPECT_FALSE(GetCommittedItemWithUniqueID(&manager_, unique_id));
  EXPECT_FALSE(GetItemWithUniqueID(&manager_, unique_id));
  EXPECT_EQ(-1, GetCommittedItemIndexWithUniqueID(&manager_, unique_id));

  // Add transient item.
  [controller_ addTransientItemWithURL:GURL("http://chromium.org")];
  item = manager_.GetTransientItem();
  EXPECT_FALSE(GetCommittedItemWithUniqueID(&manager_, unique_id));
  EXPECT_EQ(item, GetItemWithUniqueID(&manager_, unique_id));
  EXPECT_EQ(-1, GetCommittedItemIndexWithUniqueID(&manager_, unique_id));
}

}  // namespace web
