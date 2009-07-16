// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

namespace browser_defaults {

#if defined(OS_CHROMEOS)

const bool kForceAutocompleteEditFontSize = false;
const int kAutocompletePopupFontSize = 7;
const SessionStartupPref::Type kDefaultSessionStartupType =
    SessionStartupPref::LAST;
const bool kSuppressCrashInfoBar = true;
const bool kRestoreAfterCrash = true;

#elif defined(OS_LINUX)

const bool kForceAutocompleteEditFontSize = true;
const int kAutocompletePopupFontSize = 10;

#endif

#if !defined(OS_CHROMEOS)

const SessionStartupPref::Type kDefaultSessionStartupType =
    SessionStartupPref::DEFAULT;
const bool kSuppressCrashInfoBar = false;
const bool kRestoreAfterCrash = false;

#endif

}  // namespace browser_defaults
