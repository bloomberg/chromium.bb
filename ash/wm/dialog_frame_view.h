// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DIALOG_FRAME_VIEW_H_
#define ASH_WM_DIALOG_FRAME_VIEW_H_
#pragma once

#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

namespace gfx {
class Font;
}

namespace views {
class ImageButton;
}

namespace ash {
namespace internal {

// A NonClientFrameView that implements a Google-style for dialogs.
class DialogFrameView : public views::NonClientFrameView,
                        public views::ButtonListener {
 public:
  // Internal class name.
  static const char kViewClassName[];

  DialogFrameView();
  virtual ~DialogFrameView();

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // Overridden from View:
  virtual std::string GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

 private:
  gfx::Insets GetPaddingInsets() const;
  gfx::Insets GetClientInsets() const;

  gfx::Rect title_display_rect_;

  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(DialogFrameView);
};

}  // namespace internal
}  // namespace views

#endif  // ASH_WM_DIALOG_FRAME_VIEW_H_
