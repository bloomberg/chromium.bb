// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DIALOG_FRAME_VIEW_H_
#define ASH_WM_DIALOG_FRAME_VIEW_H_
#pragma once

#include "ui/views/window/non_client_view.h"

namespace gfx {
class Font;
}

namespace ash {
namespace internal {

// A NonClientFrameView that implements a Google-style for dialogs.
class DialogFrameView : public views::NonClientFrameView {
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
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;

 private:
  int GetNonClientTopHeight() const;

  gfx::Rect title_display_rect_;
  gfx::Rect close_button_rect_;

  static gfx::Font* title_font_;

  DISALLOW_COPY_AND_ASSIGN(DialogFrameView);
};

}  // namespace internal
}  // namespace views

#endif  // ASH_WM_DIALOG_FRAME_VIEW_H_
