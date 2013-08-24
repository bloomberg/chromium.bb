// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CUSTOM_FRAME_VIEW_ASH_H_
#define ASH_WM_CUSTOM_FRAME_VIEW_ASH_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/button.h"  // ButtonListener
#include "ui/views/window/non_client_view.h"

namespace ash {
class FramePainter;
}
namespace gfx {
class Font;
}
namespace views {
class ImageButton;
class Widget;
}

namespace ash {

class FrameCaptionButtonContainerView;

namespace test {
class CustomFrameViewAshTest;
}

// A NonClientFrameView used for dialogs and other non-browser windows.
// See also views::CustomFrameView and BrowserNonClientFrameViewAsh.
class ASH_EXPORT CustomFrameViewAsh : public views::NonClientFrameView {
 public:
  // Internal class name.
  static const char kViewClassName[];

  CustomFrameViewAsh();
  virtual ~CustomFrameViewAsh();

  void Init(views::Widget* frame);

  // views::NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual gfx::Size GetMaximumSize() OVERRIDE;

 private:
  friend class test::CustomFrameViewAshTest;

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  // Not owned.
  views::Widget* frame_;

  // View which contains the window controls.
  FrameCaptionButtonContainerView* caption_button_container_;

  scoped_ptr<FramePainter> frame_painter_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAsh);
};

}  // namespace ash

#endif  // ASH_WM_CUSTOM_FRAME_VIEW_ASH_H_
