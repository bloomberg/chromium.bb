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
class ToolbarModel;

namespace views {
class View;
}

// Return |view| as an OmniboxViewViews, or NULL if it is of a different type.
OmniboxViewViews* GetOmniboxViewViews(OmniboxView* view);

#if defined(OS_WIN) && !defined(USE_AURA)
// Return |view| as an OmniboxViewWin, or NULL if it is of a different type.
OmniboxViewWin* GetOmniboxViewWin(OmniboxView* view);
#endif

// Creates an OmniboxView of the appropriate type; Views or Win.
OmniboxView* CreateOmniboxView(OmniboxEditController* controller,
                               ToolbarModel* toolbar_model,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               bool popup_window_mode,
                               LocationBarView* location_bar);

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEWS_H_
