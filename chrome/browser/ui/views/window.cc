// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/window.h"

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/frame/bubble_window.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#endif  // TOOLKIT_USES_GTK

#endif  // OS_CHROMEOS

// Note: This file should be removed after the old ChromeOS frontend is removed.
//       It is not needed for Aura.
//       The visual style implemented by BubbleFrameView/BubbleWindow for
//       ChromeOS should move to Ash.
//       Calling code should just call the standard views Widget creation
//       methods and "the right thing" should just happen.
//       The remainder of the code here is dealing with the legacy CrOS WM and
//       can also be removed.

namespace browser {

views::Widget* CreateViewsWindow(gfx::NativeWindow parent,
                                 views::WidgetDelegate* delegate,
                                 DialogStyle style) {
#if defined(OS_CHROMEOS) && !defined(USE_AURA)
  return chromeos::BubbleWindow::Create(parent, style, delegate);
#else
  return views::Widget::CreateWindowWithParent(delegate, parent);
#endif
}

views::Widget* CreateViewsBubble(views::BubbleDelegateView* delegate) {
  views::Widget* bubble_widget =
      views::BubbleDelegateView::CreateBubble(delegate);
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  {
    std::vector<int> params;
    params.push_back(0);  // Do not show when screen is locked.
    chromeos::WmIpc::instance()->SetWindowType(
        bubble_widget->GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
  }
#endif
  return bubble_widget;
}

views::Widget* CreateViewsBubbleAboveLockScreen(
    views::BubbleDelegateView* delegate) {
  views::Widget* bubble_widget =
      views::BubbleDelegateView::CreateBubble(delegate);
#if defined(USE_AURA)
  ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_SettingBubbleContainer)->
          AddChild(bubble_widget->GetNativeView());
#endif
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  std::vector<int> params;
  params.push_back(1);  // Show while screen is locked.
  chromeos::WmIpc::instance()->SetWindowType(
      bubble_widget->GetNativeView(),
      chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
      &params);
#endif
  return bubble_widget;
}

}  // namespace browser
