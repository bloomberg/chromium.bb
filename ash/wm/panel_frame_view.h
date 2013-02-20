// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANEL_FRAME_VIEW_H_
#define ASH_WM_PANEL_FRAME_VIEW_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/font.h"
#include "ui/views/window/non_client_view.h"
#include "ui/views/controls/button/button.h"  // ButtonListener

namespace views {
class ImageButton;
}

namespace ash {

class FramePainter;

class ASH_EXPORT PanelFrameView : public views::NonClientFrameView,
                                  public views::ButtonListener {
 public:
  enum FrameType {
    FRAME_NONE,
    FRAME_ASH
  };

  PanelFrameView(views::Widget* frame, FrameType frame_type);
  virtual ~PanelFrameView();

 private:
  void InitFramePainter();

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
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Child View class describing the panel's title bar behavior
  // and buttons, owned by the view hierarchy
  scoped_ptr<FramePainter> frame_painter_;
  views::Widget* frame_;
  views::ImageButton* close_button_;
  views::ImageButton* minimize_button_;
  views::ImageButton* window_icon_;
  gfx::Rect client_view_bounds_;
  const gfx::Font title_font_;

  DISALLOW_COPY_AND_ASSIGN(PanelFrameView);
};

}

#endif // ASH_WM_PANEL_FRAME_VIEW_H_
