// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_views.h"

#include "base/command_line.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"
#endif

bool UseOmniboxViews() {
#if defined(OS_WIN) && !defined(USE_AURA)
  static bool kUseOmniboxViews = CommandLine::ForCurrentProcess()->
                                     HasSwitch(switches::kEnableViewsTextfield);
  return kUseOmniboxViews;
#endif
  return true;
}

OmniboxViewViews* GetOmniboxViewViews(OmniboxView* view) {
  return UseOmniboxViews() ? static_cast<OmniboxViewViews*>(view) : NULL;
}

#if defined(OS_WIN) && !defined(USE_AURA)
OmniboxViewWin* GetOmniboxViewWin(OmniboxView* view) {
  return UseOmniboxViews() ? NULL : static_cast<OmniboxViewWin*>(view);
}
#endif

OmniboxView* CreateOmniboxView(OmniboxEditController* controller,
                               ToolbarModel* toolbar_model,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               bool popup_window_mode,
                               LocationBarView* location_bar,
                               views::View* popup_parent_view) {
#if defined(OS_WIN) && !defined(USE_AURA)
  if (!UseOmniboxViews())
    return new OmniboxViewWin(controller, toolbar_model, location_bar,
                              command_updater, popup_window_mode, location_bar,
                              popup_parent_view);
#endif
  OmniboxViewViews* omnibox = new OmniboxViewViews(controller, toolbar_model,
      profile, command_updater, popup_window_mode, location_bar);
  omnibox->Init(popup_parent_view);
  return omnibox;
}
