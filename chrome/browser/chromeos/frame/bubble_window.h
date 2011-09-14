// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#pragma once

#include "chrome/browser/chromeos/frame/bubble_window_style.h"
#include "views/widget/native_widget_gtk.h"

namespace views {
class WidgetDelegate;
}

namespace chromeos {

// A window that uses BubbleFrameView as its frame.
class BubbleWindow : public views::NativeWidgetGtk {
 public:
  static views::Widget* Create(gfx::NativeWindow parent,
                               BubbleWindowStyle style,
                               views::WidgetDelegate* widget_delegate);

 protected:
  BubbleWindow(views::Widget* window, BubbleWindowStyle style);

  // Overridden from views::NativeWidgetGtk:
  virtual void InitNativeWidget(
      const views::Widget::InitParams& params) OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

 private:
  BubbleWindowStyle style_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWindow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
