// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"

namespace {
// Distance in pixels between the hotspot and when the hint should be shown.
const int kHotSpotDeltaX = 120;
const int kHotSpotDeltaY = 120;
// Size of the popup window.
const int kPopupWidth = 70;
const int kPopupHeight = 70;
}  // namespace

TEST(DockInfoTest, IsCloseToPoint) {
  bool in_enable_area;
  gfx::Point screen_loc[] = {
    gfx::Point(0, 0),
    gfx::Point(kPopupWidth/2 - 1, kPopupHeight/2 - 1),
    gfx::Point(kPopupWidth/2, kPopupHeight/2),
    gfx::Point(kHotSpotDeltaX - 1, kHotSpotDeltaY - 1),
    gfx::Point(kHotSpotDeltaX, kHotSpotDeltaY),
    gfx::Point(-kHotSpotDeltaX, -kHotSpotDeltaY)
  };
  gfx::Point hotspot[] = {
    gfx::Point(0, 0),
    gfx::Point(0, 0),
    gfx::Point(kPopupWidth, kPopupHeight),
    gfx::Point(0, 0),
    gfx::Point(2*kHotSpotDeltaX, 2*kHotSpotDeltaY),
    gfx::Point(0, 0)
  };
  bool expected_results[] = {
    true, true, true, true, false, false
  };
  bool expected_in_enable_area[] = {
    true, true, false, false, false, false
  };

  for (size_t i = 0; i < arraysize(expected_results); ++i) {
    bool result = DockInfo::IsCloseToPoint(
        screen_loc[i], hotspot[i].x(), hotspot[i].y(), &in_enable_area);
    EXPECT_EQ(expected_results[i], result);
    EXPECT_EQ(expected_in_enable_area[i], in_enable_area);
  }
}

TEST(DockInfoTest, IsCloseToMonitorPoint) {
  bool in_enable_area;
  gfx::Point screen_loc[] = {
    gfx::Point(0, 0),
    gfx::Point(kPopupWidth - 1, kPopupHeight/2 -1),
    gfx::Point(kPopupWidth, kPopupHeight/2 - 1),
    gfx::Point(kPopupWidth - 1, kPopupHeight),
    gfx::Point(2*kHotSpotDeltaX, kHotSpotDeltaY - 1),
    gfx::Point(2*kHotSpotDeltaX - 1, kHotSpotDeltaY),
    gfx::Point(2*kHotSpotDeltaX - 1, kHotSpotDeltaY),
    gfx::Point(0, 0),
    gfx::Point(kPopupWidth/2 - 1, kPopupHeight - 1),
    gfx::Point(kPopupWidth/2 - 1, kPopupHeight),
    gfx::Point(kPopupWidth/2, kPopupHeight - 1),
    gfx::Point(kHotSpotDeltaX - 1, 2*kHotSpotDeltaY),
    gfx::Point(kHotSpotDeltaX, 2*kHotSpotDeltaY - 1),
  };
  gfx::Point hotspot = gfx::Point(0, 0);
  DockInfo::Type type[] = {
    DockInfo::LEFT_HALF,
    DockInfo::LEFT_HALF,
    DockInfo::LEFT_HALF,
    DockInfo::LEFT_HALF,
    DockInfo::LEFT_HALF,
    DockInfo::LEFT_HALF,
    DockInfo::RIGHT_HALF,
    DockInfo::BOTTOM_HALF,
    DockInfo::BOTTOM_HALF,
    DockInfo::BOTTOM_HALF,
    DockInfo::BOTTOM_HALF,
    DockInfo::BOTTOM_HALF,
    DockInfo::BOTTOM_HALF,
  };
  bool expected_results[] = {
    true, true, true, true, false, false, false,
    true, true, true, true, false, false
  };
  bool expected_in_enable_area[] = {
    true, true, false, false, false, false, false,
    true, true, false, false, false, false
  };

  for (size_t i = 0; i < arraysize(expected_results); ++i) {
    bool result = DockInfo::IsCloseToMonitorPoint(
        screen_loc[i], hotspot.x(), hotspot.y(), type[i], &in_enable_area);
    EXPECT_EQ(expected_results[i], result);
    EXPECT_EQ(expected_in_enable_area[i], in_enable_area);
  }
}

TEST(DockInfoTest, IsValidForPoint) {
  DockInfo d;
  EXPECT_FALSE(d.IsValidForPoint(gfx::Point(0, 0)));
  d.set_monitor_bounds(gfx::Rect(0, 0, kPopupWidth, kPopupHeight));
  d.set_hot_spot(gfx::Point(0, 0));
  d.set_type(DockInfo::LEFT_HALF);

  gfx::Point screen_point[] = {
    gfx::Point(0, 0),
    gfx::Point(kPopupWidth + 1, kPopupHeight + 1),
    gfx::Point(2 * kHotSpotDeltaX, kHotSpotDeltaY),
  };

  bool expected_result[] = {
    true, false, false
  };

  for (size_t i = 0; i < arraysize(expected_result); ++i) {
    EXPECT_EQ(expected_result[i], d.IsValidForPoint(screen_point[i]));
  }
}

TEST(DockInfoTest, equals) {
  DockInfo d;
  DockInfo dd;
  EXPECT_TRUE(d.equals(dd));
  d.set_type(DockInfo::MAXIMIZE);
  EXPECT_FALSE(d.equals(dd));
}

TEST(DockInfoTest, CheckMonitorPoint) {
  DockInfo d;
  gfx::Point screen_loc[] = {
    gfx::Point(0, 0),
    gfx::Point(2 * kHotSpotDeltaX, kHotSpotDeltaY),
    gfx::Point(2 * kHotSpotDeltaX, kHotSpotDeltaY),
  };

  DockInfo::Type type[] = {
    DockInfo::LEFT_HALF,
    DockInfo::RIGHT_HALF,
    DockInfo::MAXIMIZE
  };

  bool expected_result[] = {
    true, false, false
  };

  for (size_t i = 0; i < arraysize(expected_result); ++i) {
    bool result = d.CheckMonitorPoint(screen_loc[i], 0, 0, type[i]);
    EXPECT_EQ(result, expected_result[i]);
    if (result == true) {
      EXPECT_EQ(0, d.hot_spot().x());
      EXPECT_EQ(0, d.hot_spot().y());
      EXPECT_EQ(type[i], d.type());
    }
  }
}

TEST(DockInfoTest, GetPopupRect) {
  DockInfo d;
  d.set_hot_spot(gfx::Point(kPopupWidth, kPopupHeight));
  DockInfo::Type type[] = {
    DockInfo::MAXIMIZE,
    DockInfo::LEFT_HALF,
    DockInfo::RIGHT_HALF,
    DockInfo::BOTTOM_HALF,
  };
  int expected_x[] = {
    kPopupWidth/2,
    kPopupWidth,
    0,
    kPopupWidth/2
  };
  int expected_y[] = {
    kPopupHeight,
    kPopupHeight/2,
    kPopupHeight/2,
    0
  };

  for (size_t i = 0; i < arraysize(type); ++i) {
    d.set_type(type[i]);
    gfx::Rect result = d.GetPopupRect();
    EXPECT_EQ(expected_x[i], result.x());
    EXPECT_EQ(expected_y[i], result.y());
    EXPECT_EQ(kPopupWidth, result.width());
    EXPECT_EQ(kPopupHeight, result.height());
  }
}
