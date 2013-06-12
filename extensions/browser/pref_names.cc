// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_names.h"

#include "base/logging.h"

namespace extensions {
namespace pref_names {

bool ScopeToPrefName(ExtensionPrefsScope scope, std::string* result) {
  switch (scope) {
    case kExtensionPrefsScopeRegular:
      *result = kPrefPreferences;
      return true;
    case kExtensionPrefsScopeRegularOnly:
      *result = kPrefRegularOnlyPreferences;
      return true;
    case kExtensionPrefsScopeIncognitoPersistent:
      *result = kPrefIncognitoPreferences;
      return true;
    case kExtensionPrefsScopeIncognitoSessionOnly:
      return false;
  }
  NOTREACHED();
  return false;
}

const char kPrefPreferences[] = "preferences";
const char kPrefIncognitoPreferences[] = "incognito_preferences";
const char kPrefRegularOnlyPreferences[] = "regular_only_preferences";
const char kPrefContentSettings[] = "content_settings";
const char kPrefIncognitoContentSettings[] = "incognito_content_settings";

}  // namespace pref_names
}  // namespace extensions
