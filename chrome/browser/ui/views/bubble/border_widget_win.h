// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_WIDGET_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_WIDGET_WIN_H_
#pragma once

#include "ui/views/bubble/bubble_border.h"
#include "views/widget/native_widget_win.h"

class BorderContents;

// This is a window that surrounds the info bubble and paints the margin and
// border.  It is a separate window so that it can be a layered window, so that
// we can use >1-bit alpha shadow images on the borders, which look nicer than
// the Windows CS_DROPSHADOW shadows.  The info bubble window itself cannot be a
// layered window because that prevents it from hosting native child controls.
class BorderWidgetWin : public views::NativeWidgetWin {
 public:
  BorderWidgetWin();
  virtual ~BorderWidgetWin() { }

  // Initializes the BrowserWidget making |owner| its owning window.
  void InitBorderWidgetWin(BorderContents* border_contents, HWND owner);

  // Given the size of the contained contents (without margins), and the rect
  // (in screen coordinates) to point to, sets the border window positions and
  // sizes the border window and returns the bounds (in screen coordinates) the
  // contents should use. |arrow_location| is prefered arrow location,
  // the function tries to preserve the location and direction, in case of RTL
  // arrow location is mirrored.
  virtual gfx::Rect SizeAndGetBounds(
      const gfx::Rect& position_relative_to,
      views::BubbleBorder::ArrowLocation arrow_location,
      const gfx::Size& contents_size);

  // Simple accessors.
  BorderContents* border_contents() { return border_contents_; }

 protected:
  BorderContents* border_contents_;

 private:
  // Overridden from NativeWidgetWin:
  virtual LRESULT OnMouseActivate(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BorderWidgetWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_WIDGET_WIN_H_
