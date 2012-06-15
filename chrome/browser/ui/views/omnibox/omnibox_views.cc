// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_views.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"
#endif

OmniboxView* CreateOmniboxView(AutocompleteEditController* controller,
                               ToolbarModel* toolbar_model,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               bool popup_window_mode,
                               LocationBarView* location_bar) {
#if defined(OS_WIN) && !defined(USE_AURA)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableViewsTextfield))
    return new OmniboxViewWin(controller, toolbar_model, location_bar,
                              command_updater, popup_window_mode, location_bar);
#endif
  OmniboxViewViews* omnibox = new OmniboxViewViews(controller, toolbar_model,
      profile, command_updater, popup_window_mode, location_bar);
  omnibox->Init();
  return omnibox;
}
