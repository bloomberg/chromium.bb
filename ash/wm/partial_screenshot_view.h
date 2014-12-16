// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_
#define ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
class ScreenshotDelegate;

// The view of taking partial screenshot, i.e.: drawing region
// rectangles during drag, and changing the mouse cursor to indicate
// the current mode.
class ASH_EXPORT PartialScreenshotView : public views::WidgetDelegateView {
 public:
  // Starts the UI for taking partial screenshot; dragging to select a region.
  // PartialScreenshotViews manage their own lifetime so caller must not delete
  // the returned PartialScreenshotViews.
  static std::vector<PartialScreenshotView*>
      StartPartialScreenshot(ScreenshotDelegate* screenshot_delegate);

 private:
  FRIEND_TEST_ALL_PREFIXES(PartialScreenshotViewTest, BasicMouse);
  FRIEND_TEST_ALL_PREFIXES(PartialScreenshotViewTest, BasicTouch);

  class OverlayDelegate;

  PartialScreenshotView(OverlayDelegate* overlay_delegate,
                        ScreenshotDelegate* screenshot_delegate);
  ~PartialScreenshotView() override;

  // Initializes partial screenshot UI widget for |root_window|.
  void Init(aura::Window* root_window);

  // Returns the currently selected region.
  gfx::Rect GetScreenshotRect() const;

  void OnSelectionStarted(const gfx::Point& position);
  void OnSelectionChanged(const gfx::Point& position);
  void OnSelectionFinished();

  // Overridden from views::View:
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  bool is_dragging_;
  gfx::Point start_position_;
  gfx::Point current_position_;

  // The delegate to receive Cancel. No ownership.
  OverlayDelegate* overlay_delegate_;

  // ScreenshotDelegate to take the actual screenshot. No ownership.
  ScreenshotDelegate* screenshot_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotView);
};

}  // namespace ash

#endif  // #ifndef ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_
