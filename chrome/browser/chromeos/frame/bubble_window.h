// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"
#include "views/window/window_gtk.h"

namespace gfx {
class Rect;
}

namespace views {
class Throbber;
class WindowDelegate;
}

namespace chromeos {

// A window that uses BubbleFrameView as its frame.
class BubbleWindow : public views::WindowGtk {
 public:
  enum Style {
    STYLE_GENERIC = 0, // Default style.
    STYLE_XBAR = 1 << 0, // Show close button at the top right (left for RTL).
    STYLE_THROBBER = 1 << 1, // Show throbber for slow rendering.
    STYLE_XSHAPE = 1 << 2 // Trim the window margins and round corners.
  };

  static views::Window* Create(gfx::NativeWindow parent,
                               const gfx::Rect& bounds,
                               Style style,
                               views::WindowDelegate* window_delegate);

  static const SkColor kBackgroundColor;

 protected:
  explicit BubbleWindow(views::WindowDelegate* window_delegate);

  // Overidden from views::WindowGtk:
  virtual void InitWindow(GtkWindow* parent, const gfx::Rect& bounds);

  // Trims the window margins and rounds off the corners.
  void TrimMargins(int margin_left, int margin_right, int margin_top,
                   int margin_bottom, int border_radius);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
