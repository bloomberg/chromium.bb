// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"
#include "views/widget/native_widget_gtk.h"

namespace views {
class Throbber;
class WidgetDelegate;
}

namespace chromeos {

// A window that uses BubbleFrameView as its frame.
class BubbleWindow : public views::NativeWidgetGtk {
 public:
  enum Style {
    STYLE_GENERIC = 0, // Default style.
    STYLE_XBAR = 1 << 0, // Show close button at the top right (left for RTL).
    STYLE_THROBBER = 1 << 1, // Show throbber for slow rendering.
    STYLE_XSHAPE = 1 << 2 // Trim the window margins and round corners.
  };

  static views::Widget* Create(gfx::NativeWindow parent,
                               Style style,
                               views::WidgetDelegate* widget_delegate);

  static const SkColor kBackgroundColor;

 protected:
  BubbleWindow(views::Widget* window, Style style);

  // Overridden from views::NativeWidgetGtk:
  virtual void InitNativeWidget(
      const views::Widget::InitParams& params) OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

  // Trims the window margins and rounds off the corners.
  void TrimMargins(int margin_left, int margin_right, int margin_top,
                   int margin_bottom, int border_radius);

 private:
  Style style_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWindow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
