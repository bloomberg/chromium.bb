// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/page_transition_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace arc {

// Tests that ShouldIgnoreNavigation returns false only for
// PAGE_TRANSITION_LINK.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithCoreTypes) {
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_TYPED));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_BOOKMARK));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_GENERATED));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_TOPLEVEL));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_RELOAD));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD_GENERATED));

  static_assert(
      ui::PAGE_TRANSITION_KEYWORD_GENERATED == ui::PAGE_TRANSITION_LAST_CORE,
      "Not all core transition types are covered here");
}

// Tests that ShouldIgnoreNavigation returns true when no qualifiers are
// provided.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithLinkWithQualifiers) {
  // The navigation is triggered by Forward or Back button.
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK)));
  // The user used the address bar to triger the navigation.
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  // The user pressed the Home button.
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_HOME_PAGE)));
  // ARC (for example) opened the link in Chrome.
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_API)));

  // Also tests the case with 2+ qualifiers.
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
      ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
}

// Just in case, does the same with ui::PAGE_TRANSITION_TYPED.
TEST(PageTransitionUtilTest,
     TestShouldIgnoreNavigationWithTypedWithQualifiers) {
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FORWARD_BACK)));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_HOME_PAGE)));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_API)));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
      ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
}

}  // namespace arc
