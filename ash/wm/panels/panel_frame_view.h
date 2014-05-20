// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANELS_PANEL_FRAME_VIEW_H_
#define ASH_WM_PANELS_PANEL_FRAME_VIEW_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class ImageView;
}

namespace ash {
class DefaultHeaderPainter;
class FrameCaptionButtonContainerView;
class FrameBorderHitTestController;

class ASH_EXPORT PanelFrameView : public views::NonClientFrameView {
 public:
  // Internal class name.
  static const char kViewClassName[];

  enum FrameType {
    FRAME_NONE,
    FRAME_ASH
  };

  PanelFrameView(views::Widget* frame, FrameType frame_type);
  virtual ~PanelFrameView();

  // Overridden from views::View:
  virtual const char* GetClassName() const OVERRIDE;

 private:
  void InitHeaderPainter();

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;

  // Overridden from views::View:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Child View class describing the panel's title bar behavior
  // and buttons, owned by the view hierarchy
  views::Widget* frame_;
  FrameCaptionButtonContainerView* caption_button_container_;
  views::ImageView* window_icon_;
  gfx::Rect client_view_bounds_;

  // Helper class for painting the header.
  scoped_ptr<DefaultHeaderPainter> header_painter_;

  // Updates the hittest bounds overrides based on the window state type.
  scoped_ptr<FrameBorderHitTestController> frame_border_hit_test_controller_;

  DISALLOW_COPY_AND_ASSIGN(PanelFrameView);
};

}

#endif // ASH_WM_PANELS_PANEL_FRAME_VIEW_H_
