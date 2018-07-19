// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_INCOGNITO_WHITELIST_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_INCOGNITO_WHITELIST_H_

#include <vector>

namespace prefs {

// Populate a whitelist of all preferences that are stored on disk in incognito
// mode. Please refer to the comments in .cc file.
void GetIncognitoWhitelist(std::vector<const char*>* whitelist);

}  // namespace prefs

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_INCOGNITO_WHITELIST_H_
