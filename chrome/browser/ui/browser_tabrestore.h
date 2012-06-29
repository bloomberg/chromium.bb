// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TABRESTORE_H_
#define CHROME_BROWSER_UI_BROWSER_TABRESTORE_H_
#pragma once

#include <vector>

#include "chrome/browser/sessions/session_types.h"

class Browser;

namespace content {
class SessionStorageNamespace;
class WebContents;
}

namespace chrome {

// Returns the index to insert a tab at during session restore and startup.
// |relative_index| gives the index of the url into the number of tabs that
// are going to be opened. For example, if three urls are passed in on the
// command line this is invoked three times with the values 0, 1 and 2.
int GetIndexForInsertionDuringRestore(Browser* browser, int relative_index);

// Add a tab with its session history restored from the SessionRestore
// system. If select is true, the tab is selected. |tab_index| gives the index
// to insert the tab at. |selected_navigation| is the index of the
// TabNavigation in |navigations| to select. If |extension_app_id| is
// non-empty the tab is an app tab and |extension_app_id| is the id of the
// extension. If |pin| is true and |tab_index|/ is the last pinned tab, then
// the newly created tab is pinned. If |from_last_session| is true,
// |navigations| are from the previous session.
content::WebContents* AddRestoredTab(
    Browser* browser,
    const std::vector<TabNavigation>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    bool select,
    bool pin,
    bool from_last_session,
    content::SessionStorageNamespace* storage_namespace);

// Replaces the state of the currently selected tab with the session
// history restored from the SessionRestore system.
void ReplaceRestoredTab(
    Browser* browser,
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    content::SessionStorageNamespace* session_storage_namespace);


}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TABRESTORE_H_
