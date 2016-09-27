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
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_TYPED, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_TYPED, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_BOOKMARK, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_SUBFRAME, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_SUBFRAME, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_MANUAL_SUBFRAME, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_MANUAL_SUBFRAME, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_GENERATED, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_GENERATED, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_TOPLEVEL, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_RELOAD, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_RELOAD, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD_GENERATED, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD_GENERATED, true));

  static_assert(
      ui::PAGE_TRANSITION_KEYWORD_GENERATED == ui::PAGE_TRANSITION_LAST_CORE,
      "Not all core transition types are covered here");
}

// Tests that ShouldIgnoreNavigation returns true when no qualifiers except
// server redirect are provided.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithLinkWithQualifiers) {
  // The navigation is triggered by Forward or Back button.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      true));
  // The user used the address bar to triger the navigation.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false));
  // The user pressed the Home button.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false));
  // ARC (for example) opened the link in Chrome.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API),
      false));
  // The navigation is triggered by a client side redirect.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false));
  // Also tests the case with 2+ qualifiers.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false));
}

// Just in case, does the same with ui::PAGE_TRANSITION_TYPED.
TEST(PageTransitionUtilTest,
     TestShouldIgnoreNavigationWithTypedWithQualifiers) {
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      true));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API),
      false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false));
}

// Tests that ShouldIgnoreNavigation returns false if SERVER_REDIRECT is the
// only qualifier given.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithServerRedirect) {
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      true));

  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_FROM_API),
      false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_FROM_API),
      true));
}

}  // namespace arc
