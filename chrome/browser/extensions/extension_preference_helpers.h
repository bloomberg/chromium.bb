// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_HELPERS_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_prefs_scope.h"

namespace extension_preference_helpers {
bool StringToScope(const std::string& s, ExtensionPrefsScope* scope);
}

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_HELPERS_H_
