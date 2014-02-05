// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(DevToolsContentsResizingStrategyTest, ApplyZero) {
  DevToolsContentsResizingStrategy zeroStrategy;
  gfx::Size container_size(100, 200);
  gfx::Rect old_devtools_bounds(0, 0, 100, 200);
  gfx::Rect old_contents_bounds(20, 20, 60, 140);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;
  ApplyDevToolsContentsResizingStrategy(
      zeroStrategy, container_size,
      old_devtools_bounds, old_contents_bounds,
      &new_devtools_bounds, &new_contents_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, ApplyInsets) {
  DevToolsContentsResizingStrategy strategy(
      gfx::Insets(10, 20, 30, 40), gfx::Size(0, 0));
  gfx::Size container_size(100, 200);
  gfx::Rect old_devtools_bounds(0, 0, 100, 200);
  gfx::Rect old_contents_bounds(20, 20, 60, 140);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;
  ApplyDevToolsContentsResizingStrategy(
      strategy, container_size,
      old_devtools_bounds, old_contents_bounds,
      &new_devtools_bounds, &new_contents_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_devtools_bounds);
  EXPECT_EQ(gfx::Rect(20, 10, 40, 160), new_contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, ApplyMinSize) {
  DevToolsContentsResizingStrategy strategy(
      gfx::Insets(10, 20, 90, 30), gfx::Size(60, 120));
  gfx::Size container_size(100, 200);
  gfx::Rect old_devtools_bounds(0, 0, 100, 200);
  gfx::Rect old_contents_bounds(20, 20, 60, 140);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;
  ApplyDevToolsContentsResizingStrategy(
      strategy, container_size,
      old_devtools_bounds, old_contents_bounds,
      &new_devtools_bounds, &new_contents_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_devtools_bounds);
  EXPECT_EQ(gfx::Rect(16, 8, 60, 120), new_contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, ApplyLargeInset) {
  DevToolsContentsResizingStrategy strategy(
      gfx::Insets(0, 130, 0, 0), gfx::Size(60, 120));
  gfx::Size container_size(100, 200);
  gfx::Rect old_devtools_bounds(0, 0, 100, 200);
  gfx::Rect old_contents_bounds(20, 20, 60, 140);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;
  ApplyDevToolsContentsResizingStrategy(
      strategy, container_size,
      old_devtools_bounds, old_contents_bounds,
      &new_devtools_bounds, &new_contents_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_devtools_bounds);
  EXPECT_EQ(gfx::Rect(40, 0, 60, 200), new_contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, ApplyTwoLargeInsets) {
  DevToolsContentsResizingStrategy strategy(
      gfx::Insets(120, 0, 80, 0), gfx::Size(60, 120));
  gfx::Size container_size(100, 200);
  gfx::Rect old_devtools_bounds(0, 0, 100, 200);
  gfx::Rect old_contents_bounds(20, 20, 60, 140);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;
  ApplyDevToolsContentsResizingStrategy(
      strategy, container_size,
      old_devtools_bounds, old_contents_bounds,
      &new_devtools_bounds, &new_contents_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 48, 100, 120), new_contents_bounds);
}

TEST(DevToolsContentsResizingStrategyTest, ApplySmallContainer) {
  DevToolsContentsResizingStrategy strategy(
      gfx::Insets(10, 10, 10, 10), gfx::Size(120, 230));
  gfx::Size container_size(100, 200);
  gfx::Rect old_devtools_bounds(0, 0, 100, 200);
  gfx::Rect old_contents_bounds(20, 20, 60, 140);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;
  ApplyDevToolsContentsResizingStrategy(
      strategy, container_size,
      old_devtools_bounds, old_contents_bounds,
      &new_devtools_bounds, &new_contents_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_devtools_bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 200), new_contents_bounds);
}
