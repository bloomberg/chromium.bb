// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

namespace browser_defaults {

#if defined(USE_AURA)
const bool kOSSupportsOtherBrowsers = false;
#else
const bool kOSSupportsOtherBrowsers = true;
#endif

#if defined(TOOLKIT_GTK)
const bool kShowCancelButtonInTaskManager = true;
#else
const bool kShowCancelButtonInTaskManager = false;
#endif

#if defined(OS_CHROMEOS)
// Make the regular omnibox text two points larger than the nine-point font
// used in the tab strip (11pt / 72pt/in * 96px/in = 14.667px).
const int kAutocompleteEditFontPixelSize = 15;

const int kAutocompleteEditFontPixelSizeInPopup = 10;

const bool kCanToggleSystemTitleBar = false;
const bool kRestorePopups = false;
const bool kShowImportOnBookmarkBar = false;
const bool kShowExitMenuItem = false;
const bool kShowFeedbackMenuItem = true;
const bool kShowHelpMenuItemIcon = true;
const bool kShowSyncSetupMenuItem = false;
const bool kShowUpgradeMenuItem = false;
const bool kDownloadPageHasShowInFolder = true;
const bool kSizeTabButtonToTopOfTabStrip = false;
const bool kSyncAutoStarts = true;
const bool kShowOtherBrowsersInAboutMemory = false;
const bool kAlwaysOpenIncognitoWindow = true;

#elif defined(TOOLKIT_GTK)

// 14px = 10.5pt @ 96dpi.
const int kAutocompleteEditFontPixelSize = 14;

// On Windows, popup windows' location text uses a font 5/6 the size of
// that in a regular window, which we duplicate here for GTK.
const int kAutocompleteEditFontPixelSizeInPopup =
    kAutocompleteEditFontPixelSize * 5.0 / 6.0;

#if defined(TOOLKIT_VIEWS)
const bool kCanToggleSystemTitleBar = false;
#else
const bool kCanToggleSystemTitleBar = true;
#endif  // defined(TOOLKIT_VIEWS)

#endif  // defined(OS_CHROMEOS)

#if defined(TOOLKIT_VIEWS)
// Windows and Chrome OS have bigger shadows in the tab art.
const int kMiniTabWidth = 64;
#else
const int kMiniTabWidth = 56;
#endif  // defined(TOOLKIT_VIEWS)

#if !defined(OS_CHROMEOS)

const bool kRestorePopups = false;
const bool kShowImportOnBookmarkBar = true;
const bool kDownloadPageHasShowInFolder = true;
#if defined(OS_MACOSX)
const bool kShowExitMenuItem = false;
#else
const bool kShowExitMenuItem = true;
#endif
const bool kShowFeedbackMenuItem = false;
const bool kShowHelpMenuItemIcon = false;
const bool kShowSyncSetupMenuItem = true;
const bool kShowUpgradeMenuItem = true;
const bool kSizeTabButtonToTopOfTabStrip = false;
#if defined(OS_ANDROID)
const bool kSyncAutoStarts = true;
const bool kShowOtherBrowsersInAboutMemory = false;
#else
const bool kSyncAutoStarts = false;
const bool kShowOtherBrowsersInAboutMemory = true;
#endif
const bool kAlwaysOpenIncognitoWindow = false;
#endif

#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
const bool kBrowserAliveWithNoWindows = true;
#else
const bool kBrowserAliveWithNoWindows = false;
#endif

const int kBookmarkBarHeight = 28;

const ui::ResourceBundle::FontStyle kAssociatedNetworkFontStyle =
    ui::ResourceBundle::BoldFont;

const int kInfoBarBorderPaddingVertical = 5;

#if defined(OS_ANDROID)
const bool kPasswordEchoEnabled = true;
#else
const bool kPasswordEchoEnabled = false;
#endif

#if defined(OS_CHROMEOS)
// On Chrome OS we're initializing into new user session with only
// Getting started guide shown as an app window (session restore is skipped).
// In all other cases (existing user) we're initializing to empty desktop
// if session restore is empty. http://crbug.com/141718
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = false;
#else
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = true;
#endif

bool bookmarks_enabled = true;

bool enable_help_app = true;

}  // namespace browser_defaults
