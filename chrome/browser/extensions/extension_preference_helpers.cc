// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_preference_helpers.h"

namespace {

const char kIncognitoPersistent[] = "incognito_persistent";
const char kIncognitoSessionOnly[] = "incognito_session_only";
const char kRegular[] = "regular";

}  // namespace

namespace extension_preference_helpers {

bool StringToScope(const std::string& s, ExtensionPrefsScope* scope) {
  if (s == kRegular)
    *scope = kExtensionPrefsScopeRegular;
  else if (s == kIncognitoPersistent)
    *scope = kExtensionPrefsScopeIncognitoPersistent;
  else if (s == kIncognitoSessionOnly)
    *scope = kExtensionPrefsScopeIncognitoSessionOnly;
  else
    return false;
  return true;
}

}  // namespace extension_preference_helpers
