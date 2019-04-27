// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using NavigationState = fuchsia::web::NavigationState;

namespace {

const char kUrl1[] = "http://www.url1.com/";
const char kUrl2[] = "http://www.url2.com/";
const char kTitle1[] = "title1";
const char kTitle2[] = "title2";

NavigationState CreateNavigationState(const GURL& url,
                                      base::StringPiece title,
                                      fuchsia::web::PageType page_type,
                                      bool can_go_back,
                                      bool can_go_forward) {
  NavigationState navigation_state;

  navigation_state.set_url(url.spec());
  navigation_state.set_title(title.as_string());
  navigation_state.set_page_type(fuchsia::web::PageType(page_type));
  navigation_state.set_can_go_back(can_go_back);
  navigation_state.set_can_go_forward(can_go_forward);

  return navigation_state;
}

}  // namespace

// Verifies that two NavigationStates that are the same are differenced
// correctly.
TEST(FrameImplUnitTest, DiffNavigationEntriesNoChange) {
  fuchsia::web::NavigationState difference;
  NavigationState state = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true);

  EXPECT_FALSE(DiffNavigationEntries(state, state, &difference));
}

// Verifies that states with different URL and title are correctly checked.
TEST(FrameImplUnitTest, DiffNavigationEntriesTitleUrl) {
  fuchsia::web::NavigationState difference;
  NavigationState state1 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true);
  NavigationState state2 = CreateNavigationState(
      GURL(kUrl2), kTitle2, fuchsia::web::PageType::NORMAL, true, true);

  bool is_changed = DiffNavigationEntries(state1, state2, &difference);

  EXPECT_TRUE(is_changed);
  EXPECT_TRUE(difference.has_title());
  EXPECT_EQ(difference.title(), kTitle2);
  EXPECT_TRUE(difference.has_url());
  EXPECT_EQ(difference.url(), kUrl2);
}

// Verifies that states with different can_go_back and can_go_forward are
// correctly checked.
TEST(FrameImplUnitTest, DiffNavigationEntriesGoBackAndForward) {
  fuchsia::web::NavigationState difference;
  NavigationState state1 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, false);
  NavigationState state2 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, false, true);

  bool is_changed = DiffNavigationEntries(state1, state2, &difference);

  EXPECT_TRUE(difference.has_can_go_back());
  EXPECT_TRUE(difference.has_can_go_back());
  EXPECT_TRUE(is_changed);
  EXPECT_TRUE(difference.can_go_forward());
  EXPECT_FALSE(difference.can_go_back());
}
