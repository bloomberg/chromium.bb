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
class WindowDelegate;
}

namespace chromeos {

// A window that uses BubbleFrameView as its frame.
class BubbleWindow : public views::WindowGtk {
 public:
  static views::Window* Create(gfx::NativeWindow parent,
                               const gfx::Rect& bounds,
                               views::WindowDelegate* window_delegate);

  static const SkColor kBackgroundColor;

 protected:
  explicit BubbleWindow(views::WindowDelegate* window_delegate);

  // Overidden from views::WindowGtk:
  virtual void Init(GtkWindow* parent, const gfx::Rect& bounds);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
