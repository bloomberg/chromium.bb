// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_
#define ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_
#pragma once

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
class ScreenshotDelegate;

// The view of taking partial screenshot, i.e.: drawing region
// rectangles during drag, and changing the mouse cursor to indicate
// the current mode.
class ASH_EXPORT PartialScreenshotView : public views::WidgetDelegateView {
 public:
  PartialScreenshotView(ScreenshotDelegate* screenshot_delegate);
  virtual ~PartialScreenshotView();

  // Starts the UI for taking partial screenshot; dragging to select a region.
  static void StartPartialScreenshot(ScreenshotDelegate* screenshot_delegate);

  // Cancels the current screenshot UI.
  void Cancel();

  // Overriddden from View:
  virtual gfx::NativeCursor GetCursor(const views::MouseEvent& event) OVERRIDE;

 private:
  gfx::Rect GetScreenshotRect() const;

  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const views::MouseWheelEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;

  bool is_dragging_;
  gfx::Point start_position_;
  gfx::Point current_position_;
  ScreenshotDelegate* screenshot_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotView);
};

}  // namespace ash

#endif  // #ifndef ASH_WM_PARTIAL_SCREENSHOT_VIEW_H_
