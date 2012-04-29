// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/window.h"

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/widget/widget.h"

// Note: This file should be removed after the old ChromeOS frontend is removed.
//       It is not needed for Aura.
//       The visual style implemented by BubbleFrameView/BubbleWindow for
//       ChromeOS should move to Ash.
//       Calling code should just call the standard views Widget creation
//       methods and "the right thing" should just happen.
//       The remainder of the code here is dealing with the legacy CrOS WM and
//       can also be removed.

namespace browser {

views::Widget* CreateFramelessViewsWindow(gfx::NativeWindow parent,
                                          views::WidgetDelegate* delegate) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = delegate;
  // Will this function be called if !defined(USE_AURA)?
#if defined(OS_WIN) || defined(USE_AURA)
  params.parent = parent;
#endif
  // No frame so does not need params.transparent = true
  widget->Init(params);
  return widget;
}

}  // namespace browser
