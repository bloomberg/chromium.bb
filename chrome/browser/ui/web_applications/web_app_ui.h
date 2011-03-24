// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_H_
#pragma once

#include <vector>

#include "chrome/browser/shell_integration.h"

class TabContentsWrapper;

namespace web_app {

// Extracts shortcut info of given TabContents.
void GetShortcutInfoForTab(TabContentsWrapper* tab_contents,
                           ShellIntegration::ShortcutInfo* info);

// Updates web app shortcut of the TabContents. This function checks and
// updates web app icon and shortcuts if needed. For icon, the check is based
// on MD5 hash of icon image. For shortcuts, it checks the desktop, start menu
// and quick launch (as well as pinned shortcut) for shortcut and only
// updates (recreates) them if they exits.
void UpdateShortcutForTabContents(TabContentsWrapper* tab_contents);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_H_
