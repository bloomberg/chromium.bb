// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/toolkit_extra_parts.h"
#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/ui_base_switches.h"

ChromeBrowserMainExtraPartsViews::ChromeBrowserMainExtraPartsViews() {
}

void ChromeBrowserMainExtraPartsViews::ToolkitInitialized() {
  // The delegate needs to be set before any UI is created so that windows
  // display the correct icon.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ChromeViewsDelegate;
}

void ChromeBrowserMainExtraPartsViews::PreCreateThreads() {
  // Enable the new style dialogs when using the interactive autocomplete
  // dialog. Modifying the command line is only safe before starting threads.
  // It also has to come after about flags modifies the command line, which
  // is why it's not possible to do this in ToolkitInitialized.
  // TODO(estade): remove this.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableInteractiveAutocomplete))
    command_line->AppendSwitch(switches::kEnableNewDialogStyle);
}

namespace chrome {

void AddViewsToolkitExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsViews());
}

}  // namespace chrome
