// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_PREFS_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_PREFS_H_

#include <cstddef>

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/dom_distiller/core/pref_names.h"

// A preference exposed to Java.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.preferences
enum Pref {
  ALLOW_DELETING_BROWSER_HISTORY,
  INCOGNITO_MODE_AVAILABILITY,
  READER_FOR_ACCESSIBILITY_ENABLED,
  // PREF_NUM_PREFS must be the last entry.
  PREF_NUM_PREFS
};

// The indices must match value of Pref.
// Remember to update prefs_unittest.cc as well.
const char* const kPrefsExposedToJava[] = {
    prefs::kAllowDeletingBrowserHistory, prefs::kIncognitoModeAvailability,
    dom_distiller::prefs::kReaderForAccessibility,
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_PREFS_H_
