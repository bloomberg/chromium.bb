// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/navigation_manager_util.h"

#import "ios/web/public/navigation_item.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class NavigationManagerUtilTest : public PlatformTest {
 protected:
  web::TestNavigationManager nav_manager_;
};

// Tests that empty navigation manager returns nullptr.
TEST_F(NavigationManagerUtilTest, TestLastNonRedirectedItemEmpty) {
  EXPECT_FALSE(GetLastCommittedNonRedirectedItem(nullptr));
  EXPECT_FALSE(GetLastCommittedNonRedirectedItem(&nav_manager_));
}

// Tests that typed in URL works correctly.
TEST_F(NavigationManagerUtilTest, TestLastNonRedirectedItemTypedUrl) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_TYPED);
  web::NavigationItem* item = GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_TYPED));
}

// Tests that link click works correctly.
TEST_F(NavigationManagerUtilTest, TestLastNonRedirectedItemLinkClicked) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_LINK));
}

// Tests that redirect items are skipped.
TEST_F(NavigationManagerUtilTest, TestLastNonRedirectedItemLinkMultiRedirects) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_LINK);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  web::NavigationItem* item = GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_LINK));
}

// Tests that when all items are redirects, nullptr is returned.
TEST_F(NavigationManagerUtilTest, TestLastNonRedirectedItemAllRedirects) {
  nav_manager_.AddItem(GURL("http://bar.com/redir0"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  web::NavigationItem* item = GetLastCommittedNonRedirectedItem(&nav_manager_);
  EXPECT_FALSE(item);
}

// Tests that earlier redirects are not found.
TEST_F(NavigationManagerUtilTest, TestLastNonRedirectedItemNotEarliest) {
  nav_manager_.AddItem(GURL("http://foo.com/bookmark"),
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_TYPED);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  web::NavigationItem* item = GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_TYPED));
}
