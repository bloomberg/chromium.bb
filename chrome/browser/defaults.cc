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

#if defined(TOOLKIT_USES_GTK)
const bool kShowCancelButtonInTaskManager = true;
#else
const bool kShowCancelButtonInTaskManager = false;
#endif

#if defined(OS_CHROMEOS)
// Make the regular omnibox text two points larger than the nine-point font
// used in the tab strip (11pt / 72pt/in * 96px/in = 14.667px).
const int kAutocompleteEditFontPixelSize = 15;

const int kAutocompleteEditFontPixelSizeInPopup = 10;

const int kMiniTabWidth = 64;
const bool kCanToggleSystemTitleBar = false;
const bool kRestorePopups = false;
const bool kShowImportOnBookmarkBar = false;
const bool kShowExitMenuItem = true;
const bool kDownloadPageHasShowInFolder = true;
const bool kSizeTabButtonToTopOfTabStrip = false;
const bool kSyncAutoStarts = true;
const bool kShowOtherBrowsersInAboutMemory = false;
const bool kAlwaysOpenIncognitoWindow = true;

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
#endif  // defined(TOOLKIT_VIEWS)

#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)

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
const bool kSyncAutoStarts = false;
const bool kShowOtherBrowsersInAboutMemory = true;
const bool kAlwaysOpenIncognitoWindow = false;
#endif

#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
const bool kBrowserAliveWithNoWindows = true;
#else
const bool kBrowserAliveWithNoWindows = false;
#endif

const int kBookmarkBarHeight = 28;
const int kNewtabBookmarkBarHeight = 57;

const ui::ResourceBundle::FontStyle kAssociatedNetworkFontStyle =
    ui::ResourceBundle::BoldFont;

const int kInfoBarBorderPaddingVertical = 5;

#if defined(OS_ANDROID)
const bool kPasswordEchoEnabled = true;
#else
const bool kPasswordEchoEnabled = false;
#endif

bool bookmarks_enabled = true;

bool enable_help_app = true;

}  // namespace browser_defaults
