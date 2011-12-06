// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
#pragma once

#include "chrome/browser/ui/dialog_style.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

extern const SkColor kBubbleWindowBackgroundColor;

// A window that uses BubbleFrameView as its frame.
class BubbleWindow : public views::Widget {
 public:
  static views::Widget* Create(gfx::NativeWindow parent,
                               DialogStyle style,
                               views::WidgetDelegate* widget_delegate);

  virtual ~BubbleWindow();

  // Overridden from views::Widget:
  virtual views::NonClientFrameView* CreateNonClientFrameView() OVERRIDE;

 private:
  explicit BubbleWindow(DialogStyle style);

  DialogStyle style_;

  DISALLOW_COPY_AND_ASSIGN(BubbleWindow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_H_
