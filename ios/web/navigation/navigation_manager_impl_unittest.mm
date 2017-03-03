// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/test/test_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {
// Stub class for NavigationManagerDelegate.
class TestNavigationManagerDelegate : public NavigationManagerDelegate {
  void GoToIndex(int index) override {}
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override {}
  void OnNavigationItemsPruned(size_t pruned_item_count) override {}
  void OnNavigationItemChanged() override{};
  void OnNavigationItemCommitted(const LoadCommittedDetails&) override {}
  WebState* GetWebState() override { return nullptr; }
};
}  // namespace

// Test fixture for NavigationManagerImpl testing.
class NavigationManagerTest : public PlatformTest {
 protected:
  NavigationManagerTest() : manager_(new NavigationManagerImpl()) {
    manager_->SetDelegate(&delegate_);
    manager_->SetBrowserState(&browser_state_);
    controller_.reset([[CRWSessionController alloc]
        initWithBrowserState:&browser_state_
                 openedByDOM:NO]);
    manager_->SetSessionController(controller_.get());
  }
  CRWSessionController* session_controller() { return controller_.get(); }
  NavigationManagerImpl* navigation_manager() { return manager_.get(); }

 private:
  TestBrowserState browser_state_;
  TestNavigationManagerDelegate delegate_;
  std::unique_ptr<NavigationManagerImpl> manager_;
  base::scoped_nsobject<CRWSessionController> controller_;
};

// Tests state of an empty navigation manager.
TEST_F(NavigationManagerTest, EmptyManager) {
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  EXPECT_EQ(-1, navigation_manager()->GetCurrentItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetCurrentItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(0));
}

// Tests that GetPendingItemIndex() returns -1 if there is no pending entry.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithoutPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns current item index if there is a
// pending entry.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns same index as was set by
// -[CRWSessionController setPendingItemIndex:].
TEST_F(NavigationManagerTest, GetPendingItemIndexWithIndexedPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  [session_controller() setPendingItemIndex:0];
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that going back or negative offset is not possible without a committed
// item.
TEST_F(NavigationManagerTest, CanGoBackWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is a
// transient item, but not committed items.
TEST_F(NavigationManagerTest, CanGoBackWithTransientItem) {
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com")];

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is possible if there is a transient
// item and at least one committed item.
TEST_F(NavigationManagerTest, CanGoBackWithTransientItemAndCommittedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com/0")];

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is ony one
// committed item and no transient item.
TEST_F(NavigationManagerTest, CanGoBackWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests going back possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoBackWithMultipleCommitedItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToItemAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToItemAtIndex:0];
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToItemAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going forward or positive offset is not possible if there is a
// pending entry.
TEST_F(NavigationManagerTest, CanGoForwardWithPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  [session_controller() goToItemAtIndex:0];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);

  // Pending entry should not allow going forward.
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible without a
// committed item.
TEST_F(NavigationManagerTest, CanGoForwardWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible if there is ony
// one committed item and no transient item.
TEST_F(NavigationManagerTest, CanGoForwardWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests going forward possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoForwardWithMultipleCommitedEntries) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:0];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:1];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:2];
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests CanGoToOffset API for positive, negative and zero delta. Tested
// navigation manager will have redirect entries to make sure they are
// appropriately skipped.
TEST_F(NavigationManagerTest, OffsetsWithoutPendingIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_IS_REDIRECT_MASK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_IS_REDIRECT_MASK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());

  // Go to entry at index 1 and test API from that state.
  [session_controller() goToItemAtIndex:1];
  ASSERT_EQ(1, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(3));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(3));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(INT_MIN, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-1000000000, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000002, navigation_manager()->GetIndexForOffset(1000000000));

  // Go to entry at index 2 and test API from that state.
  [session_controller() goToItemAtIndex:2];
  ASSERT_EQ(2, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483647, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999999, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000003, navigation_manager()->GetIndexForOffset(1000000000));

  // Go to entry at index 4 and test API from that state.
  [session_controller() goToItemAtIndex:4];
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483646, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999998, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Test with existing transient entry.
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com")];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-3));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-3));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483645, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999997, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Now test with pending item index.
  [session_controller() discardNonCommittedItems];

  // Set pending index to 1 and test API from that state.
  [session_controller() setPendingItemIndex:1];
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(3));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(3));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(INT_MIN, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-1000000000, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000002, navigation_manager()->GetIndexForOffset(1000000000));

  // Set pending index to 2 and test API from that state.
  [session_controller() setPendingItemIndex:2];
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(2, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483647, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999999, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000003, navigation_manager()->GetIndexForOffset(1000000000));

  // Set pending index to 4 and committed entry to 1 and test.
  [session_controller() goToItemAtIndex:1];
  [session_controller() setPendingItemIndex:4];
  ASSERT_EQ(1, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(4, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483646, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999998, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Test with existing transient entry in the end of the stack.
  [session_controller() goToItemAtIndex:4];
  [session_controller() setPendingItemIndex:-1];
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com")];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-3));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-3));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483645, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999997, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));
}

// Tests offsets with pending transient entries (specifically gong back and
// forward from a pending navigation entry that is added to the middle of the
// navigation stack).
TEST_F(NavigationManagerTest, OffsetsWithPendingTransientEntry) {
  // Create a transient item in the middle of the navigation stack and go back
  // to it (pending index is 1, current index is 2).
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com/1")];
  [session_controller() setPendingItemIndex:1];

  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_EQ(2, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));

  // Now go forward to that middle transient item (pending index is 1,
  // current index is 0).
  [session_controller() goToItemAtIndex:0];
  [session_controller() setPendingItemIndex:1];
  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_EQ(0, navigation_manager()->GetCurrentItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));
}

// Tests that desktop user agent can be enforced to use for next pending item.
TEST_F(NavigationManagerTest, OverrideDesktopUserAgent) {
  navigation_manager()->OverrideDesktopUserAgentForNextPendingItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  NavigationItem* visible_item = navigation_manager()->GetVisibleItem();
  EXPECT_EQ(visible_item->GetUserAgentType(), UserAgentType::DESKTOP);
}

// Tests that the UserAgentType is propagated to subsequent NavigationItems.
TEST_F(NavigationManagerTest, UserAgentTypePropagation) {
  // Add and commit two NavigationItems.
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::MOBILE, item1->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* item2 = navigation_manager()->GetLastCommittedItem();

  // Verify that the second item's UserAgentType is equal to the first.
  EXPECT_EQ(item1->GetUserAgentType(), item2->GetUserAgentType());

  // Update |item2|'s UA type to DESKTOP and commit a new item.
  item2->SetUserAgentType(web::UserAgentType::DESKTOP);
  ASSERT_EQ(web::UserAgentType::DESKTOP, item2->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.3.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* item3 = navigation_manager()->GetLastCommittedItem();

  // Verify that the third item's UserAgentType is equal to the second.
  EXPECT_EQ(item2->GetUserAgentType(), item3->GetUserAgentType());
}

// Tests that the UserAgentType is propagated to subsequent NavigationItems if
// a native URL exists in between naviations.
TEST_F(NavigationManagerTest, UserAgentTypePropagationPastNativeItems) {
  // GURL::Replacements that will replace a GURL's scheme with the test native
  // scheme.
  GURL::Replacements native_scheme_replacement;
  native_scheme_replacement.SetSchemeStr(kTestNativeContentScheme);

  // Create two non-native navigations that are separated by a native one.
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::MOBILE, item1->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      item1->GetURL().ReplaceComponents(native_scheme_replacement), Referrer(),
      ui::PAGE_TRANSITION_TYPED, web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* native_item1 =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE, native_item1->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* item2 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item1|'s UserAgentType is propagated to |item2|.
  EXPECT_EQ(item1->GetUserAgentType(), item2->GetUserAgentType());

  // Update |item2|'s UA type to DESKTOP and add a third non-native navigation,
  // once again separated by a native one.
  item2->SetUserAgentType(web::UserAgentType::DESKTOP);
  ASSERT_EQ(web::UserAgentType::DESKTOP, item2->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      item2->GetURL().ReplaceComponents(native_scheme_replacement), Referrer(),
      ui::PAGE_TRANSITION_TYPED, web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* native_item2 =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE, native_item2->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.3.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED);
  [session_controller() commitPendingItem];
  web::NavigationItem* item3 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item2|'s UserAgentType is propagated to |item3|.
  EXPECT_EQ(item2->GetUserAgentType(), item3->GetUserAgentType());
}

}  // namespace web
