// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/window.h"

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

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
  return CreateFramelessWindowWithParentAndBounds(delegate,
      parent, gfx::Rect());
}

views::Widget* CreateFramelessWindowWithParentAndBounds(
    views::WidgetDelegate* delegate,
    gfx::NativeWindow parent,
    const gfx::Rect& bounds) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = delegate;
  // Will this function be called if !defined(USE_AURA)?
#if defined(OS_WIN) || defined(USE_AURA)
  params.parent = parent;
#endif
  // No frame so does not need params.transparent = true
  params.bounds = bounds;
  widget->Init(params);
  return widget;
}

views::Widget* CreateViewsBubbleAboveLockScreen(
    views::BubbleDelegateView* delegate) {
#if defined(USE_AURA)
  delegate->set_parent_window(
      ash::Shell::GetInstance()->GetContainer(
          ash::internal::kShellWindowId_SettingBubbleContainer));
#endif
  views::Widget* bubble_widget =
      views::BubbleDelegateView::CreateBubble(delegate);
  return bubble_widget;
}

}  // namespace browser
