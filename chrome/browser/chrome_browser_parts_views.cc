// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_parts_views.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "ui/views/widget/widget.h"

ChromeBrowserPartsViews::ChromeBrowserPartsViews()
    : content::BrowserMainParts() {
}

void ChromeBrowserPartsViews::ToolkitInitialized() {
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ChromeViewsDelegate;

  // TODO(beng): Move to WidgetImpl and implement on Windows too!
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDebugViewsPaint))
    views::Widget::SetDebugPaintEnabled(true);
}

void ChromeBrowserPartsViews::PreMainMessageLoopRun() {
}

bool ChromeBrowserPartsViews::MainMessageLoopRun(int* result_code) {
  return false;
}
