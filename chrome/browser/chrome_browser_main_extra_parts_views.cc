// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_views.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "ui/views/desktop/desktop_window_view.h"
#include "views/widget/widget.h"

ChromeBrowserMainExtraPartsViews::ChromeBrowserMainExtraPartsViews()
    : ChromeBrowserMainExtraParts() {
}

void ChromeBrowserMainExtraPartsViews::ToolkitInitialized() {
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ChromeViewsDelegate;

  // TODO(beng): Move to WidgetImpl and implement on Windows too!
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDebugViewsPaint))
    views::Widget::SetDebugPaintEnabled(true);
}

void ChromeBrowserMainExtraPartsViews::PostBrowserProcessInit() {
#if !defined(USE_AURA)
  views::Widget::SetPureViews(
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kUsePureViews));
  // Launch the views desktop shell window and register it as the default parent
  // for all unparented views widgets.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kViewsDesktop)) {
    std::string desktop_type_cmd =
        command_line.GetSwitchValueASCII(switches::kViewsDesktop);
    if (desktop_type_cmd != "disabled") {
      views::desktop::DesktopWindowView::DesktopType desktop_type;
      if (desktop_type_cmd == "netbook")
        desktop_type = views::desktop::DesktopWindowView::DESKTOP_NETBOOK;
      else if (desktop_type_cmd == "other")
        desktop_type = views::desktop::DesktopWindowView::DESKTOP_OTHER;
      else
        desktop_type = views::desktop::DesktopWindowView::DESKTOP_DEFAULT;
      views::desktop::DesktopWindowView::CreateDesktopWindow(desktop_type);
      ChromeViewsDelegate* chrome_views_delegate = static_cast
          <ChromeViewsDelegate*>(views::ViewsDelegate::views_delegate);
      chrome_views_delegate->default_parent_view =
        views::desktop::DesktopWindowView::desktop_window_view;
    }
  }
#endif
}
