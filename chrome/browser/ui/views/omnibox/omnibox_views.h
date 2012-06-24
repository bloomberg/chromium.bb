// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEWS_H_
#pragma once

class CommandUpdater;
class LocationBarView;
class OmniboxEditController;
class OmniboxView;
class Profile;
class ToolbarModel;

namespace views {
class View;
}

OmniboxView* CreateOmniboxView(OmniboxEditController* controller,
                               ToolbarModel* toolbar_model,
                               Profile* profile,
                               CommandUpdater* command_updater,
                               bool popup_window_mode,
                               LocationBarView* location_bar,
                               views::View* popup_parent_view);

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEWS_H_
