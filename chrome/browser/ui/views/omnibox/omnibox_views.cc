// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_views.h"

#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "ui/views/controls/textfield/textfield.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"
#endif

OmniboxViewViews* GetOmniboxViewViews(OmniboxView* view) {
  return views::Textfield::IsViewsTextfieldEnabled() ?
      static_cast<OmniboxViewViews*>(view) : NULL;
}

OmniboxViewWin* GetOmniboxViewWin(OmniboxView* view) {
#if defined(OS_WIN) && !defined(USE_AURA)
  return views::Textfield::IsViewsTextfieldEnabled() ?
      NULL : static_cast<OmniboxViewWin*>(view);
#else
  return NULL;
#endif
}

OmniboxView* CreateOmniboxView(OmniboxEditController* controller,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               bool popup_window_mode,
                               LocationBarView* location_bar,
                               const gfx::FontList& font_list,
                               int font_y_offset) {
#if defined(OS_WIN) && !defined(USE_AURA)
  if (!views::Textfield::IsViewsTextfieldEnabled()) {
    return new OmniboxViewWin(controller, location_bar, command_updater,
                              popup_window_mode, font_list, font_y_offset);
  }
#endif
  OmniboxViewViews* omnibox = new OmniboxViewViews(
      controller, profile, command_updater, popup_window_mode, location_bar,
      font_list, font_y_offset);
  omnibox->Init();
  return omnibox;
}
