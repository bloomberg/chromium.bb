// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PREF_NAMES_H_
#define EXTENSIONS_BROWSER_PREF_NAMES_H_

#include <string>

#include "extensions/browser/extension_prefs_scope.h"

namespace extensions {

// Preference keys which are needed by both the ExtensionPrefs and by external
// clients, such as APIs.
namespace pref_names {

// If the given |scope| is persisted, return true and populate |result| with the
// appropriate pref name. If |scope| is not persisted, return false, and leave
// |result| unchanged.
bool ScopeToPrefName(ExtensionPrefsScope scope, std::string* result);

// A preference that contains any extension-controlled preferences.
extern const char kPrefPreferences[];

// A preference that contains any extension-controlled incognito preferences.
extern const char kPrefIncognitoPreferences[];

// A preference that contains any extension-controlled regular-only preferences.
extern const char kPrefRegularOnlyPreferences[];

// A preference that contains extension-set content settings.
extern const char kPrefContentSettings[];

// A preference that contains extension-set content settings.
extern const char kPrefIncognitoContentSettings[];

}  // namespace pref_names

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PREF_NAMES_H_
