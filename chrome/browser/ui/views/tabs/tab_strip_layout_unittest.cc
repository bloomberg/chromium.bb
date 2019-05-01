// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Returns a string with the width of each gfx::Rect in |tab_bounds|, separated
// by spaces.
std::string TabWidthsAsString(const std::vector<gfx::Rect>& tab_bounds) {
  std::string result;
  for (const auto& bounds : tab_bounds) {
    if (!result.empty())
      result += " ";
    result += base::NumberToString(bounds.width());
  }
  return result;
}

// Returns a string with the x-coordinate of each gfx::Rect in |tab_bounds|,
// separated by spaces.
std::string TabXPositionsAsString(const std::vector<gfx::Rect>& tab_bounds) {
  std::string result;
  for (const auto& bounds : tab_bounds) {
    if (!result.empty())
      result += " ";
    result += base::NumberToString(bounds.x());
  }
  return result;
}

struct TestCase {
  int num_pinned_tabs = 0;
  int num_tabs = 0;
  int active_index = 0;
  int tabstrip_width = 0;
};

struct CalculateTabBoundsResult {
  std::vector<gfx::Rect> bounds;
  int active_width;
  int inactive_width;
};

constexpr int kStandardWidth = 100;
constexpr int kStandardHeight = 10;
constexpr int kMinActiveWidth = 20;
constexpr int kMinInactiveWidth = 14;
constexpr int kPinnedWidth = 10;
constexpr int kTabOverlap = 4;

CalculateTabBoundsResult CalculateTabBounds(TestCase test_case) {
  TabSizeInfo size_info;
  size_info.pinned_tab_width = kPinnedWidth;
  size_info.min_active_width = kMinActiveWidth;
  size_info.min_inactive_width = kMinInactiveWidth;
  size_info.standard_size = gfx::Size(kStandardWidth, kStandardHeight);
  size_info.tab_overlap = kTabOverlap;

  std::vector<TabAnimationState> ideal_animation_states;
  for (int tab_index = 0; tab_index < test_case.num_tabs; tab_index++) {
    ideal_animation_states.push_back(TabAnimationState::ForIdealTabState(
        TabAnimationState::TabOpenness::kOpen,
        tab_index < test_case.num_pinned_tabs
            ? TabAnimationState::TabPinnedness::kPinned
            : TabAnimationState::TabPinnedness::kUnpinned,
        tab_index == test_case.active_index
            ? TabAnimationState::TabActiveness::kActive
            : TabAnimationState::TabActiveness::kInactive,
        0));
  }

  int active_width;
  int inactive_width;
  std::vector<gfx::Rect> tab_bounds = CalculateTabBounds(
      size_info, ideal_animation_states, test_case.tabstrip_width,
      &active_width, &inactive_width);

  CalculateTabBoundsResult result;
  result.bounds = tab_bounds;
  result.active_width = active_width;
  result.inactive_width = inactive_width;
  return result;
}

}  // namespace

// These tests verify that layout behaves correctly in various situations. In
// particular we want layout to adhere to the following constraints:
// * Tabs are the standard size given by TabSizeInfo when there's room.
// * Tabs are never smaller than the minimum sizes given by TabSizeInfo, even if
//   there isn't enough room.
// * Pinned tabs are always the width given by TabSizeInfo.
// * Remainder pixels (leftover when the available width is distributed evenly)
//   are distributed from left to right.
// * And otherwise tabs shrink to fit the available width.

TEST(TabStripLayoutTest, Basics) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_tabs = 3;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("100 100 100", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 96 192", TabXPositionsAsString(result.bounds));
  EXPECT_EQ(kStandardWidth, result.active_width);
  EXPECT_EQ(kStandardWidth, result.inactive_width);
  for (const auto& bounds : result.bounds) {
    EXPECT_EQ(0, bounds.y());
    EXPECT_EQ(kStandardHeight, bounds.height());
  }
}

TEST(TabStripLayoutTest, AllPinnedTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_pinned_tabs = test_case.num_tabs = 3;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 10 10", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 6 12", TabXPositionsAsString(result.bounds));
}

TEST(TabStripLayoutTest, MixedPinnedAndNormalTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 1000;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 100 100", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 6 102", TabXPositionsAsString(result.bounds));
}

TEST(TabStripLayoutTest, MiddleWidth) {
  TestCase test_case;
  test_case.tabstrip_width = 100;
  test_case.num_tabs = 4;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("28 28 28 28", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 24 48 72", TabXPositionsAsString(result.bounds));
  EXPECT_EQ(28, result.active_width);
  EXPECT_EQ(28, result.inactive_width);
}

TEST(TabStripLayoutTest, MiddleWidthAndPinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 100;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 49 49", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 6 51", TabXPositionsAsString(result.bounds));
}

TEST(TabStripLayoutTest, MiddleWidthRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 102;
  test_case.num_tabs = 4;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("29 29 28 28", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 25 50 74", TabXPositionsAsString(result.bounds));
  EXPECT_EQ(28, result.active_width);
  EXPECT_EQ(28, result.inactive_width);
}

TEST(TabStripLayoutTest, MiddleWidthRoundedAndPinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 101;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 50 49", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 6 52", TabXPositionsAsString(result.bounds));
  EXPECT_EQ(49, result.active_width);
  EXPECT_EQ(49, result.inactive_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthOneTab) {
  TestCase test_case;
  test_case.tabstrip_width = 15;
  test_case.num_tabs = 1;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("20", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0", TabXPositionsAsString(result.bounds));
  EXPECT_EQ(kMinActiveWidth, result.active_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidth) {
  TestCase test_case;
  test_case.tabstrip_width = 90;
  test_case.num_tabs = 6;
  test_case.active_index = 3;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("18 18 18 20 18 18", TabWidthsAsString(result.bounds));
  EXPECT_EQ("0 14 28 42 58 72", TabXPositionsAsString(result.bounds));
  EXPECT_EQ(18, result.inactive_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 93;
  test_case.num_tabs = 6;
  test_case.active_index = 3;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("19 19 19 20 18 18", TabWidthsAsString(result.bounds));
  EXPECT_EQ(18, result.inactive_width);
}

TEST(TabStripLayoutTest, BelowMinActiveWidthActivePinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 85;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 19 19 19 19 19", TabWidthsAsString(result.bounds));
}

TEST(TabStripLayoutTest, BelowMinActiveWidthInactivePinnedTab) {
  TestCase test_case;
  test_case.tabstrip_width = 82;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;
  test_case.active_index = 2;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 18 20 18 18 18", TabWidthsAsString(result.bounds));
}

TEST(TabStripLayoutTest, BelowMinActiveWidthActivePinnedTabRounded) {
  TestCase test_case;
  test_case.tabstrip_width = 86;
  test_case.num_tabs = 6;
  test_case.num_pinned_tabs = 1;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 20 19 19 19 19", TabWidthsAsString(result.bounds));
}

TEST(TabStripLayoutTest, NotEnoughSpace) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("20 14 14", TabWidthsAsString(result.bounds));
  EXPECT_EQ(kMinActiveWidth, result.active_width);
  EXPECT_EQ(kMinInactiveWidth, result.inactive_width);
}

TEST(TabStripLayoutTest, NotEnoughSpaceAllPinnedTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 3;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 10 10", TabWidthsAsString(result.bounds));
}

TEST(TabStripLayoutTest, NotEnoughSpaceMixedPinnedAndNormalTabs) {
  TestCase test_case;
  test_case.tabstrip_width = 10;
  test_case.num_tabs = 3;
  test_case.num_pinned_tabs = 1;

  auto result = CalculateTabBounds(test_case);
  EXPECT_EQ("10 14 14", TabWidthsAsString(result.bounds));
}

TEST(TabStripLayoutTest, ExactlyEnoughSpaceAllPinnedTabs) {
  TestCase test_case;
  test_case.num_tabs = 2;
  test_case.num_pinned_tabs = 2;
  test_case.tabstrip_width = 2 * kPinnedWidth - kTabOverlap;

  // We want to check the case where the necessary strip width equals the
  // available width.
  auto result = CalculateTabBounds(test_case);

  EXPECT_EQ("10 10", TabWidthsAsString(result.bounds));

  // Validate that the tabstrip width is indeeed exactly enough to hold two
  // pinned tabs.
  EXPECT_EQ(test_case.tabstrip_width, result.bounds[1].right());
}
