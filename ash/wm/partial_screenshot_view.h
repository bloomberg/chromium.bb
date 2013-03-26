// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_
#define ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class RootWindow;
}

namespace ash {
class ScreenshotDelegate;

// The view of taking partial screenshot, i.e.: drawing region
// rectangles during drag, and changing the mouse cursor to indicate
// the current mode.
class ASH_EXPORT PartialScreenshotView : public views::WidgetDelegateView {
 public:
  // Starts the UI for taking partial screenshot; dragging to select a region.
  static void StartPartialScreenshot(ScreenshotDelegate* screenshot_delegate);

 private:
  class OverlayDelegate;

  PartialScreenshotView(OverlayDelegate* overlay_delegate,
                        ScreenshotDelegate* screenshot_delegate);
  virtual ~PartialScreenshotView();

  // Initializes partial screenshot UI widget for |root_window|.
  void Init(aura::RootWindow* root_window);

  // Returns the currently selected region.
  gfx::Rect GetScreenshotRect() const;

  void OnSelectionStarted(const gfx::Point& position);
  void OnSelectionChanged(const gfx::Point& position);
  void OnSelectionFinished();

  // Overridden from views::View:
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

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
