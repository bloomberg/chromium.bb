// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/border_widget_win.h"

#include <windows.h>

#include "chrome/browser/ui/views/bubble/border_contents.h"

BorderWidgetWin::BorderWidgetWin()
    : border_contents_(NULL) {
  set_window_style(WS_POPUP);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
}

void BorderWidgetWin::Init(BorderContents* border_contents, HWND owner) {
  DCHECK(!border_contents_);
  border_contents_ = border_contents;
  border_contents_->Init();
  WidgetWin::Init(owner, gfx::Rect());
  SetContentsView(border_contents_);
  SetWindowPos(owner, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW);
}

gfx::Rect BorderWidgetWin::SizeAndGetBounds(
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    const gfx::Size& contents_size) {
  // Ask the border view to calculate our bounds (and our contents').
  gfx::Rect contents_bounds;
  gfx::Rect window_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to, arrow_location,
                                     false, contents_size, &contents_bounds,
                                     &window_bounds);
  SetBounds(window_bounds);

  // Return |contents_bounds| in screen coordinates.
  contents_bounds.Offset(window_bounds.origin());
  return contents_bounds;
}

LRESULT BorderWidgetWin::OnMouseActivate(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  // Never activate.
  return MA_NOACTIVATE;
}
