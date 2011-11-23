// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

namespace browser_defaults {

#if defined(USE_AURA) || defined(OS_CHROMEOS)
const bool kOSSupportsOtherBrowsers = false;
#else
const bool kOSSupportsOtherBrowsers = true;
#endif

#if defined(OS_CHROMEOS)

// Make the regular omnibox text two points larger than the nine-point font
// used in the tab strip (11pt / 72pt/in * 96px/in = 14.667px).
const int kAutocompleteEditFontPixelSize = 15;

const int kAutocompleteEditFontPixelSizeInPopup = 10;

const SessionStartupPref::Type kDefaultSessionStartupType =
    SessionStartupPref::LAST;
const int kMiniTabWidth = 64;
const bool kCanToggleSystemTitleBar = false;
const bool kRestorePopups = false;
const bool kShowImportOnBookmarkBar = false;
const bool kShowExitMenuItem = true;
const bool kDownloadPageHasShowInFolder = true;
const bool kSizeTabButtonToTopOfTabStrip = true;
const bool kBootstrapSyncAuthentication = true;
const bool kShowOtherBrowsersInAboutMemory = false;
const bool kAlwaysOpenIncognitoWindow = true;
const bool kShowCancelButtonInTaskManager = true;

#elif defined(TOOLKIT_USES_GTK)

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
#endif

#endif

#if !defined(OS_CHROMEOS)

const SessionStartupPref::Type kDefaultSessionStartupType =
    SessionStartupPref::DEFAULT;
const int kMiniTabWidth = 56;
const bool kRestorePopups = false;
const bool kShowImportOnBookmarkBar = true;
const bool kDownloadPageHasShowInFolder = true;
#if defined(OS_MACOSX)
const bool kShowExitMenuItem = false;
#else
const bool kShowExitMenuItem = true;
#endif
const bool kSizeTabButtonToTopOfTabStrip = false;
const bool kBootstrapSyncAuthentication = false;
const bool kShowOtherBrowsersInAboutMemory = true;
const bool kAlwaysOpenIncognitoWindow = false;
const bool kShowCancelButtonInTaskManager = false;
#endif

#if defined(OS_MACOSX)
const bool kBrowserAliveWithNoWindows = true;
#else
const bool kBrowserAliveWithNoWindows = false;
#endif

const int kBookmarkBarHeight = 28;
const int kNewtabBookmarkBarHeight = 57;
const int kMaxTabCount = INT_MAX;

const ui::ResourceBundle::FontStyle kAssociatedNetworkFontStyle =
    ui::ResourceBundle::BoldFont;

const int kInfoBarBorderPaddingVertical = 5;

bool bookmarks_enabled = true;

bool skip_restore = false;

bool enable_help_app = true;

}  // namespace browser_defaults
