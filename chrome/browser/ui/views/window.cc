// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/window.h"

#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/frame/bubble_window.h"
#endif  // defined(OS_CHROMEOS)

namespace browser {

views::Widget* CreateViewsWindow(gfx::NativeWindow parent,
                                 views::WidgetDelegate* delegate) {
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  // TODO(msw): revert to BubbleWindow for all ChromeOS cases when CL
  // for crbug.com/98322 is landed.
  return chromeos::BubbleWindow::Create(parent,
      STYLE_GENERIC, delegate);
#else
  return views::Widget::CreateWindowWithParent(delegate, parent);
#endif
}

}  // namespace browser
