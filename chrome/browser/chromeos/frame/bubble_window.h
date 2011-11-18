// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#pragma once

#include "chrome/browser/chromeos/frame/bubble_window_style.h"
#include "chrome/browser/ui/dialog_style.h"

#if defined(TOOLKIT_USES_GTK)
// TODO(msw): While I dislike the includes and code to be mixed into the same
// preprocessor conditional, this seems okay as I can hopefully fix this up
// in a matter of days / crbug.com/98322.
#include "views/widget/native_widget_gtk.h"
#else // TOOLKIT_USES_GTK
#include "views/view.h"
#endif

namespace views {
class WidgetDelegate;
}

#if defined(TOOLKIT_USES_GTK)
// TODO(msw): To fix as explained above (crbug.com/98322).
namespace chromeos {

// A window that uses BubbleFrameView as its frame.
class BubbleWindow : public views::NativeWidgetGtk {
 public:
  static views::Widget* Create(gfx::NativeWindow parent,
                               DialogStyle style,
                               views::WidgetDelegate* widget_delegate);

 protected:
  BubbleWindow(views::Widget* window, DialogStyle style);

  // Overridden from views::NativeWidgetGtk:
  virtual void InitNativeWidget(
      const views::Widget::InitParams& params) OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

 private:
  DialogStyle style_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWindow);
};

}  // namespace chromeos

#else // TOOLKIT_USES_GTK

namespace chromeos {

class BubbleWindow {
 public:
  static views::Widget* Create(gfx::NativeWindow parent,
                               DialogStyle style,
                               views::WidgetDelegate* widget_delegate) {
    NOTIMPLEMENTED();
    return NULL;
  }
};

}  // namespace chromeos

#endif // TOOLKIT_USES_GTK

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
