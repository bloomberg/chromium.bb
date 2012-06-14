// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_SCOPE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_SCOPE_H_
#pragma once

#include "base/basictypes.h"

// Scope for a preference.
enum ExtensionPrefsScope {
  // Regular profile and incognito.
  kExtensionPrefsScopeRegular,
  // Regular profile only.
  kExtensionPrefsScopeRegularOnly,
  // Incognito profile; preference is persisted to disk and remains active
  // after a browser restart.
  kExtensionPrefsScopeIncognitoPersistent,
  // Incognito profile; preference is kept in memory and deleted when the
  // incognito session is terminated.
  kExtensionPrefsScopeIncognitoSessionOnly
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_SCOPE_H_
