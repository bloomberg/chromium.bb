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
const bool kSuppressCrashInfoBar = true;
const bool kRestoreAfterCrash = true;

#elif defined(OS_LINUX)

// 13.4px = 10pt @ 96dpi.
const double kAutocompleteEditFontPixelSize = 13.4;

// On Windows, popup windows' autocomplete box have a font 5/6 the size of a
// regular window, which we duplicate here for GTK.
const double kAutocompleteEditFontPixelSizeInPopup =
    kAutocompleteEditFontPixelSize * 5.0 / 6.0;

const int kAutocompletePopupFontSize = 10;

#endif

#if !defined(OS_CHROMEOS)

const SessionStartupPref::Type kDefaultSessionStartupType =
    SessionStartupPref::DEFAULT;
const bool kSuppressCrashInfoBar = false;
const bool kRestoreAfterCrash = false;

#endif

}  // namespace browser_defaults
