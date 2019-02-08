// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_transform_window.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

float GetItemScale(const gfx::Rect& source,
                   const gfx::Rect& target,
                   int top_view_inset,
                   int title_height) {
  return ScopedOverviewTransformWindow::GetItemScale(
      source.size(), target.size(), top_view_inset, title_height);
}

}  // namespace

using ScopedOverviewTransformWindowTest = AshTestBase;

// Tests that transformed Rect scaling preserves its aspect ratio. The window
// scale is determined by the target height and so the test is actually testing
// that the width is calculated correctly. Since all calculations are done with
// floating point values and then safely converted to integers (using ceiled and
// floored values where appropriate), the  expectations are forgiving (use
// *_NEAR) within a single pixel.
TEST_F(ScopedOverviewTransformWindowTest, TransformedRectMaintainsAspect) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(10, 10, 100, 100));
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());

  gfx::Rect rect(50, 50, 200, 400);
  gfx::Rect bounds(100, 100, 50, 50);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  float scale = GetItemScale(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 400, 200);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 25, 25);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 25, 50);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 50, 25);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered.
TEST_F(ScopedOverviewTransformWindowTest, TransformedRectIsCentered) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(10, 10, 100, 100));
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  gfx::Rect rect(50, 50, 200, 400);
  gfx::Rect bounds(100, 100, 50, 50);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_GE(transformed_rect.x(), bounds.x());
  EXPECT_LE(transformed_rect.right(), bounds.right());
  EXPECT_GE(transformed_rect.y(), bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(transformed_rect.y() - bounds.y(),
              bounds.bottom() - transformed_rect.bottom(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered
// when inset and header height are specified.
TEST_F(ScopedOverviewTransformWindowTest, TransformedRectIsCenteredWithInset) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(10, 10, 100, 100));
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  gfx::Rect rect(50, 50, 400, 200);
  gfx::Rect bounds(100, 100, 50, 50);
  const int inset = 20;
  const int header_height = 10;
  const float scale = GetItemScale(rect, bounds, inset, header_height);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, inset,
                                                            header_height);
  // The |rect| width does not fit and therefore it gets centered outside
  // |bounds| starting before |bounds.x()| and ending after |bounds.right()|.
  EXPECT_LE(transformed_rect.x(), bounds.x());
  EXPECT_GE(transformed_rect.right(), bounds.right());
  EXPECT_GE(
      transformed_rect.y() + gfx::ToCeiledInt(scale * inset) - header_height,
      bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(
      transformed_rect.y() + (int)(scale * inset) - header_height - bounds.y(),
      bounds.bottom() - transformed_rect.bottom(), 1);
}

// Verify that a window which will be displayed like a letter box on the window
// grid has the correct bounds.
TEST_F(ScopedOverviewTransformWindowTest, TransformingLetteredRect) {
  // Create a window whose width is more than twice the height.
  const gfx::Rect original_bounds(10, 10, 300, 100);
  const int scale = 3;
  std::unique_ptr<aura::Window> window = CreateTestWindow(original_bounds);
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  EXPECT_EQ(ScopedOverviewTransformWindow::GridWindowFillMode::kLetterBoxed,
            transform_window.type());

  // Without any headers, the width should match the target, and the height
  // should be such that the aspect ratio of |original_bounds| is maintained.
  const gfx::Rect overview_bounds(0, 0, 100, 100);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(
          original_bounds, overview_bounds, 0, 0);
  EXPECT_EQ(overview_bounds.width(), transformed_rect.width());
  EXPECT_NEAR(overview_bounds.height() / scale, transformed_rect.height(), 1);

  // With headers, the width should still match the target. The height should
  // still be such that the aspect ratio is maintained, but the original header
  // which is hidden in overview needs to be accounted for.
  const int original_header = 10;
  const int overview_header = 20;
  transformed_rect = transform_window.ShrinkRectToFitPreservingAspectRatio(
      original_bounds, overview_bounds, original_header, overview_header);
  EXPECT_EQ(overview_bounds.width(), transformed_rect.width());
  EXPECT_NEAR((overview_bounds.height() - original_header) / scale,
              transformed_rect.height() - original_header / scale, 1);
  EXPECT_TRUE(overview_bounds.Contains(transformed_rect));

  // Verify that for an extreme window, the transform window stores the
  // original overview item bounds, minus the header.
  gfx::Rect new_overview_bounds = overview_bounds;
  new_overview_bounds.Inset(0, overview_header, 0, 0);
  ASSERT_TRUE(transform_window.overview_bounds().has_value());
  EXPECT_EQ(transform_window.overview_bounds().value(), new_overview_bounds);
}

// Verify that a window which will be displayed like a pillar box on the window
// grid has the correct bounds.
TEST_F(ScopedOverviewTransformWindowTest, TransformingPillaredRect) {
  // Create a window whose height is more than twice the width.
  const gfx::Rect original_bounds(10, 10, 100, 300);
  const int scale = 3;
  std::unique_ptr<aura::Window> window = CreateTestWindow(original_bounds);
  ScopedOverviewTransformWindow transform_window(nullptr, window.get());
  EXPECT_EQ(ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed,
            transform_window.type());

  // Without any headers, the height should match the target, and the width
  // should be such that the aspect ratio of |original_bounds| is maintained.
  const gfx::Rect overview_bounds(0, 0, 100, 100);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(
          original_bounds, overview_bounds, 0, 0);
  EXPECT_EQ(overview_bounds.height(), transformed_rect.height());
  EXPECT_NEAR(overview_bounds.width() / scale, transformed_rect.width(), 1);

  // With headers, the height should not include the area reserved for the
  // overview window title. It also needs to account for the original header
  // which will become hidden in overview mode.
  const int original_header = 10;
  const int overview_header = 20;
  transformed_rect = transform_window.ShrinkRectToFitPreservingAspectRatio(
      original_bounds, overview_bounds, original_header, overview_header);
  EXPECT_NEAR(overview_bounds.height() - overview_header,
              transformed_rect.height() - original_header / scale, 1);
  EXPECT_TRUE(overview_bounds.Contains(transformed_rect));

  // Verify that for an extreme window, the transform window stores the
  // original overview item bounds, minus the header.
  gfx::Rect new_overview_bounds = overview_bounds;
  new_overview_bounds.Inset(0, overview_header, 0, 0);
  ASSERT_TRUE(transform_window.overview_bounds().has_value());
  EXPECT_EQ(transform_window.overview_bounds().value(), new_overview_bounds);
}

// Tests the cases when very wide or tall windows enter overview mode.
TEST_F(ScopedOverviewTransformWindowTest, ExtremeWindowBounds) {
  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Window |wide|, with size (400, 160) will be resized to (200, 160)
  // when the 400x200 is rotated to 200x400, and should be considered a normal
  // overview window after display change.
  UpdateDisplay("400x200");
  std::unique_ptr<aura::Window> wide =
      CreateTestWindow(gfx::Rect(10, 10, 400, 160));
  std::unique_ptr<aura::Window> tall =
      CreateTestWindow(gfx::Rect(10, 10, 50, 200));
  std::unique_ptr<aura::Window> normal =
      CreateTestWindow(gfx::Rect(10, 10, 200, 200));

  ScopedOverviewTransformWindow scoped_wide(nullptr, wide.get());
  ScopedOverviewTransformWindow scoped_tall(nullptr, tall.get());
  ScopedOverviewTransformWindow scoped_normal(nullptr, normal.get());

  // Verify the window dimension type is as expected after entering overview
  // mode.
  using GridWindowFillMode = ScopedOverviewTransformWindow::GridWindowFillMode;
  EXPECT_EQ(GridWindowFillMode::kLetterBoxed, scoped_wide.type());
  EXPECT_EQ(GridWindowFillMode::kPillarBoxed, scoped_tall.type());
  EXPECT_EQ(GridWindowFillMode::kNormal, scoped_normal.type());

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  scoped_wide.UpdateWindowDimensionsType();
  scoped_tall.UpdateWindowDimensionsType();
  scoped_normal.UpdateWindowDimensionsType();

  // Verify that |wide| has its window dimension type updated after the display
  // change.
  EXPECT_EQ(GridWindowFillMode::kNormal, scoped_wide.type());
  EXPECT_EQ(GridWindowFillMode::kPillarBoxed, scoped_tall.type());
  EXPECT_EQ(GridWindowFillMode::kNormal, scoped_normal.type());
}

// Verify that if the window's bounds are changed while it's in overview mode,
// the rounded edge mask's bounds are also changed accordingly.
TEST_F(ScopedOverviewTransformWindowTest, WindowBoundsChangeTest) {
  UpdateDisplay("400x400");
  const gfx::Rect bounds(10, 10, 200, 200);
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
  ScopedOverviewTransformWindow scoped_window(nullptr, window.get());
  scoped_window.UpdateMask(true);

  EXPECT_TRUE(scoped_window.mask_);
  EXPECT_EQ(window->bounds(), scoped_window.GetMaskBoundsForTesting());
  EXPECT_EQ(bounds, scoped_window.GetMaskBoundsForTesting());

  wm::GetWindowState(window.get())->Maximize();
  EXPECT_EQ(window->bounds(), scoped_window.GetMaskBoundsForTesting());
  EXPECT_NE(bounds, scoped_window.GetMaskBoundsForTesting());
}

}  // namespace ash
