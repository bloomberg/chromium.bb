// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/legacy_navigation_manager_impl.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/test/test_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// URL scheme that will be rewritten by UrlRewriter installed in
// NavigationManagerTest fixture. Scheme will be changed to kTestWebUIScheme.
const char kSchemeToRewrite[] = "navigationmanagerschemetorewrite";

// Replaces |kSchemeToRewrite| scheme with |kTestWebUIScheme|.
bool UrlRewriter(GURL* url, BrowserState* browser_state) {
  if (url->scheme() == kSchemeToRewrite) {
    GURL::Replacements scheme_replacements;
    scheme_replacements.SetSchemeStr(kTestWebUIScheme);
    *url = url->ReplaceComponents(scheme_replacements);
  }
  return false;
}

// Stub class for NavigationManagerDelegate.
class TestNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  bool reload_called() { return reload_called_; }

 private:
  // NavigationManagerDelegate overrides.
  void GoToIndex(int index) override {}
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override {}
  void Reload() override { reload_called_ = true; }
  void OnNavigationItemsPruned(size_t pruned_item_count) override {}
  void OnNavigationItemChanged() override {}
  void OnNavigationItemCommitted(const LoadCommittedDetails&) override {}
  WebState* GetWebState() override { return nullptr; }

  bool reload_called_ = false;
};
}  // namespace

// Programmatic test fixture for NavigationManagerImpl testing.
// GetParam() chooses whether to run tests on LegacyNavigationManagerImpl or
// (the soon-to-be-implemented) WKBasedNavigationManagerImpl.
// TODO(crbug.com/734150): cleanup the LegacyNavigationManagerImpl use case.
class NavigationManagerTest : public PlatformTest,
                              public ::testing::WithParamInterface<bool> {
 protected:
  NavigationManagerTest() {
    bool test_legacy_navigation_manager = GetParam();
    if (test_legacy_navigation_manager) {
      manager_.reset(new LegacyNavigationManagerImpl);
    } else {
      DCHECK(false) << "Not implemented.";
    }
    // Setup rewriter.
    BrowserURLRewriter::GetInstance()->AddURLRewriter(UrlRewriter);
    url::AddStandardScheme(kSchemeToRewrite, url::SCHEME_WITHOUT_PORT);

    manager_->SetDelegate(&delegate_);
    manager_->SetBrowserState(&browser_state_);
    controller_ =
        [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
    manager_->SetSessionController(controller_);
  }
  CRWSessionController* session_controller() { return controller_; }
  NavigationManagerImpl* navigation_manager() { return manager_.get(); }

  TestNavigationManagerDelegate navigation_manager_delegate() {
    return delegate_;
  }

 private:
  TestBrowserState browser_state_;
  TestNavigationManagerDelegate delegate_;
  std::unique_ptr<NavigationManagerImpl> manager_;
  CRWSessionController* controller_;
};

// Tests state of an empty navigation manager.
TEST_P(NavigationManagerTest, EmptyManager) {
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(0));
}

// Tests that GetPendingItemIndex() returns -1 if there is no pending entry.
TEST_P(NavigationManagerTest, GetPendingItemIndexWithoutPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns current item index if there is a
// pending entry.
TEST_P(NavigationManagerTest, GetPendingItemIndexWithPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns same index as was set by
// -[CRWSessionController setPendingItemIndex:].
TEST_P(NavigationManagerTest, GetPendingItemIndexWithIndexedPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  [session_controller() setPendingItemIndex:0];
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that going back or negative offset is not possible without a committed
// item.
TEST_P(NavigationManagerTest, CanGoBackWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is a
// transient item, but not committed items.
TEST_P(NavigationManagerTest, CanGoBackWithTransientItem) {
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com")];

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is possible if there is a transient
// item and at least one committed item.
TEST_P(NavigationManagerTest, CanGoBackWithTransientItemAndCommittedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com/0")];

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is ony one
// committed item and no transient item.
TEST_P(NavigationManagerTest, CanGoBackWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests going back possibility with multiple committed items.
TEST_P(NavigationManagerTest, CanGoBackWithMultipleCommitedItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToItemAtIndex:0 discardNonCommittedItems:NO];
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));

  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going forward or positive offset is not possible without a
// committed item.
TEST_P(NavigationManagerTest, CanGoForwardWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible if there is ony
// one committed item and no transient item.
TEST_P(NavigationManagerTest, CanGoForwardWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests going forward possibility with multiple committed items.
TEST_P(NavigationManagerTest, CanGoForwardWithMultipleCommitedEntries) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:0 discardNonCommittedItems:NO];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  [session_controller() goToItemAtIndex:2 discardNonCommittedItems:NO];
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests CanGoToOffset API for positive, negative and zero delta. Tested
// navigation manager will have redirect entries to make sure they are
// appropriately skipped.
TEST_P(NavigationManagerTest, OffsetsWithoutPendingIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());

  // Go to entry at index 1 and test API from that state.
  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
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
  [session_controller() goToItemAtIndex:2 discardNonCommittedItems:NO];
  ASSERT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
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
  [session_controller() goToItemAtIndex:4 discardNonCommittedItems:NO];
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  [session_controller() setPendingItemIndex:4];
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
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
  [session_controller() goToItemAtIndex:4 discardNonCommittedItems:NO];
  [session_controller() setPendingItemIndex:-1];
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com")];
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
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
TEST_P(NavigationManagerTest, OffsetsWithPendingTransientEntry) {
  // Create a transient item in the middle of the navigation stack and go back
  // to it (pending index is 1, current index is 2).
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  [session_controller() addTransientItemWithURL:GURL("http://www.url.com/1")];
  [session_controller() setPendingItemIndex:1];

  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));

  // Now go forward to that middle transient item (pending index is 1,
  // current index is 0).
  [session_controller() goToItemAtIndex:0 discardNonCommittedItems:NO];
  [session_controller() setPendingItemIndex:1];
  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));
}

// Tests that when given a pending item, adding a new pending item replaces the
// existing pending item if their URLs are different.
TEST_P(NavigationManagerTest, ReplacePendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(existing_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if new pending item is not a
// form submission.
TEST_P(NavigationManagerTest, NotReplaceSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if new pending item is a form
// submission while existing pending item is not.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if the user agent override
// option is INHERIT.
TEST_P(NavigationManagerTest, NotReplaceSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// DESKTOP.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// MOBILE.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item
// succeeds if the new item's URL is different from the last committed item.
TEST_P(NavigationManagerTest, AddPendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(existing_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the new item is not form submission.
TEST_P(NavigationManagerTest, NotAddSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the new item is a form submission while the last
// committed item is not.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  // Add if new transition is a form submission.
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if both the new item and the last committed item are form
// submissions.
TEST_P(NavigationManagerTest,
       NotAddSameUrlPendingItemIfDuplicateFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the user agent override option is INHERIT.
TEST_P(NavigationManagerTest, NotAddSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is DESKTOP.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is MOBILE.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that desktop user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is DESKTOP.
TEST_P(NavigationManagerTest, OverrideUserAgentWithDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  [session_controller() commitPendingItem];
  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  EXPECT_EQ(UserAgentType::MOBILE, last_committed_item->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that mobile user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is MOBILE.
TEST_P(NavigationManagerTest, OverrideUserAgentWithMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  last_committed_item->SetUserAgentType(UserAgentType::DESKTOP);
  EXPECT_EQ(UserAgentType::DESKTOP, last_committed_item->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that the UserAgentType of an INHERIT item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterInherit) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType of a MOBILE item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType of a DESKTOP item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType is propagated to subsequent NavigationItems if
// a native URL exists in between naviations.
TEST_P(NavigationManagerTest, UserAgentTypePropagationPastNativeItems) {
  // GURL::Replacements that will replace a GURL's scheme with the test native
  // scheme.
  GURL::Replacements native_scheme_replacement;
  native_scheme_replacement.SetSchemeStr(kTestNativeContentScheme);

  // Create two non-native navigations that are separated by a native one.
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  web::NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::MOBILE, item1->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      item1->GetURL().ReplaceComponents(native_scheme_replacement), Referrer(),
      ui::PAGE_TRANSITION_TYPED, web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  web::NavigationItem* native_item1 =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE, native_item1->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
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
      ui::PAGE_TRANSITION_TYPED, web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  web::NavigationItem* native_item2 =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE, native_item2->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.3.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];
  web::NavigationItem* item3 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item2|'s UserAgentType is propagated to |item3|.
  EXPECT_EQ(item2->GetUserAgentType(), item3->GetUserAgentType());
}

// Tests that adding transient item for a pending item with mobile user agent
// type results in a transient item with mobile user agent type.
TEST_P(NavigationManagerTest, AddTransientItemForMobilePendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::MOBILE);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetTransientItem()->GetUserAgentType());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that adding transient item for a pending item with desktop user agent
// type results in a transient item with desktop user agent type.
TEST_P(NavigationManagerTest, AddTransientItemForDesktopPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::DESKTOP);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetTransientItem()->GetUserAgentType());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL is no-op when there
// are no transient, pending and committed items.
TEST_P(NavigationManagerTest, ReloadEmptyWithNormalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_FALSE(navigation_manager_delegate().reload_called());

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the renderer initiated pending item unchanged when there is one.
TEST_P(NavigationManagerTest, ReloadRendererPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the user initiated pending item unchanged when there is one.
TEST_P(NavigationManagerTest, ReloadUserPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item.
TEST_P(NavigationManagerTest, ReloadLastCommittedItemWithNormalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item, but there
// forward items after last committed item.
TEST_P(NavigationManagerTest,
       ReloadLastCommittedItemWithNormalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL is
// no-op when there are no transient, pending and committed items.
TEST_P(NavigationManagerTest, ReloadEmptyWithOriginalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_FALSE(navigation_manager_delegate().reload_called());

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the renderer initiated pending item's url to its original request url
// when there is one.
TEST_P(NavigationManagerTest, ReloadRendererPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the user initiated pending item's url to its original request url
// when there is one.
TEST_P(NavigationManagerTest, ReloadUserPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item.
TEST_P(NavigationManagerTest, ReloadLastCommittedItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);
  [session_controller() commitPendingItem];

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item, but there are forward items after last committed item.
TEST_P(NavigationManagerTest,
       ReloadLastCommittedItemWithOriginalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);
  [session_controller() commitPendingItem];

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [session_controller() commitPendingItem];

  [session_controller() goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);
  EXPECT_TRUE(navigation_manager_delegate().reload_called());

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that app-specific URLs are not rewritten for renderer-initiated loads
// unless requested by a page with app-specific url.
TEST_P(NavigationManagerTest, RewritingAppSpecificUrls) {
  // URL should not be rewritten as there is no committed URL.
  GURL url1(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url1, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url1, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten because last committed URL is not app-specific.
  [session_controller() commitPendingItem];
  GURL url2(url::SchemeHostPort(kSchemeToRewrite, "test2", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url2, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten for user initiated navigations.
  GURL url3(url::SchemeHostPort(kSchemeToRewrite, "test3", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url3, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url3(
      url::SchemeHostPort(kTestWebUIScheme, "test3", 0).Serialize());
  EXPECT_EQ(rewritten_url3, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten because last committed URL is app-specific.
  [session_controller() commitPendingItem];
  GURL url4(url::SchemeHostPort(kSchemeToRewrite, "test4", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url4, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url4(
      url::SchemeHostPort(kTestWebUIScheme, "test4", 0).Serialize());
  EXPECT_EQ(rewritten_url4, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetIndexOfItem() returns the correct values.
TEST_P(NavigationManagerTest, GetIndexOfItem) {
  // Create two items and add them to the NavigationManagerImpl.
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->CommitPendingItem();
  web::NavigationItem* item0 = navigation_manager()->GetLastCommittedItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->CommitPendingItem();
  web::NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  // Create an item that does not exist in the NavigationManagerImpl.
  std::unique_ptr<web::NavigationItem> item_not_found =
      web::NavigationItem::Create();
  // Verify GetIndexOfItem() results.
  EXPECT_EQ(0, navigation_manager()->GetIndexOfItem(item0));
  EXPECT_EQ(1, navigation_manager()->GetIndexOfItem(item1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexOfItem(item_not_found.get()));
}

INSTANTIATE_TEST_CASE_P(
    ProgrammaticNavigationManagerTest,
    NavigationManagerTest,
    ::testing::Values(true /* test_legacy_navigation_manager */));

}  // namespace web
