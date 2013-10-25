// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEWS_H_

class CommandUpdater;
class LocationBarView;
class OmniboxEditController;
class OmniboxView;
class OmniboxViewViews;
class OmniboxViewWin;
class Profile;

namespace gfx {
class FontList;
}

namespace views {
class View;
}

// Return |view| as an OmniboxViewViews, or NULL if it is of a different type.
OmniboxViewViews* GetOmniboxViewViews(OmniboxView* view);

// Return |view| as an OmniboxViewWin, or NULL if it is of a different type.
OmniboxViewWin* GetOmniboxViewWin(OmniboxView* view);

// Creates an OmniboxView of the appropriate type; Views or Win.
OmniboxView* CreateOmniboxView(OmniboxEditController* controller,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               bool popup_window_mode,
                               LocationBarView* location_bar,
                               const gfx::FontList& font_list,
                               int font_y_offset);

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEWS_H_
