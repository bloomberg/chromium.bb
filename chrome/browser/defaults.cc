// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

namespace browser_defaults {

#if defined(USE_X11)
#if defined(TOOLKIT_VIEWS)
const bool kCanToggleSystemTitleBar = false;
#else
const bool kCanToggleSystemTitleBar = true;
#endif
#endif

const int kOmniboxFontPixelSize = 16;

#if defined(USE_AURA)
#if defined(OS_WIN)
const bool kShowLinkDisambiguationPopup = true;
#else
const bool kShowLinkDisambiguationPopup = false;
#endif
#endif

#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
const bool kBrowserAliveWithNoWindows = true;
const bool kShowExitMenuItem = false;
#else
const bool kBrowserAliveWithNoWindows = false;
const bool kShowExitMenuItem = true;
#endif

#if defined(OS_CHROMEOS)
const bool kShowHelpMenuItemIcon = true;
const bool kShowUpgradeMenuItem = false;
const bool kShowImportOnBookmarkBar = false;
const bool kAlwaysOpenIncognitoWindow = true;
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = false;
#else
const bool kShowHelpMenuItemIcon = false;
const bool kShowUpgradeMenuItem = true;
const bool kShowImportOnBookmarkBar = true;
const bool kAlwaysOpenIncognitoWindow = false;
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = true;
#endif

const bool kDownloadPageHasShowInFolder = true;
const bool kSizeTabButtonToTopOfTabStrip = false;

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
const bool kSyncAutoStarts = true;
const bool kShowOtherBrowsersInAboutMemory = false;
#else
const bool kSyncAutoStarts = false;
const bool kShowOtherBrowsersInAboutMemory = true;
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
const bool kScrollEventChangesTab = true;
#else
const bool kScrollEventChangesTab = false;
#endif

const ui::ResourceBundle::FontStyle kAssociatedNetworkFontStyle =
    ui::ResourceBundle::BoldFont;

#if !defined(OS_ANDROID)
const bool kPasswordEchoEnabled = false;
#endif

bool bookmarks_enabled = true;

bool enable_help_app = true;

}  // namespace browser_defaults
