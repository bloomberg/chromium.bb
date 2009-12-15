// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

namespace browser_defaults {

#if defined(OS_CHROMEOS)

const double kAutocompleteEditFontPixelSize = 12.0;
const double kAutocompleteEditFontPixelSizeInPopup = kAutocompletePopupFontSize;

const int kAutocompletePopupFontSize = 7;
const SessionStartupPref::Type kDefaultSessionStartupType =
    SessionStartupPref::LAST;
const int kPinnedTabWidth = 64;
const bool kCanToggleSystemTitleBar = false;
const bool kRestorePopups = true;
const bool kShowImportOnBookmarkBar = false;
const bool kShowExitMenuItem = false;
const bool kShowAboutMenuItem = true;
const bool kOSSupportsOtherBrowsers = false;

#elif defined(OS_LINUX)

// 13.4px = 10pt @ 96dpi.
const double kAutocompleteEditFontPixelSize = 13.4;

// On Windows, popup windows' autocomplete box have a font 5/6 the size of a
// regular window, which we duplicate here for GTK.
const double kAutocompleteEditFontPixelSizeInPopup =
    kAutocompleteEditFontPixelSize * 5.0 / 6.0;

const int kAutocompletePopupFontSize = 10;

#if defined(TOOLKIT_VIEWS)
const bool kCanToggleSystemTitleBar = false;
#else
const bool kCanToggleSystemTitleBar = true;
#endif

#endif

#if !defined(OS_CHROMEOS)

const SessionStartupPref::Type kDefaultSessionStartupType =
    SessionStartupPref::DEFAULT;
const int kPinnedTabWidth = 56;
const bool kRestorePopups = false;
const bool kShowImportOnBookmarkBar = true;
#if defined(OS_MACOSX)
const bool kShowExitMenuItem = false;
const bool kShowAboutMenuItem = false;
#else
const bool kShowExitMenuItem = true;
const bool kShowAboutMenuItem = true;
#endif
const bool kOSSupportsOtherBrowsers = true;

#endif

#if defined(OS_MACOSX)
const bool kBrowserAliveWithNoWindows = true;
#else
const bool kBrowserAliveWithNoWindows = false;
#endif

}  // namespace browser_defaults
