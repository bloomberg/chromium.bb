// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for fileBrowserPrivate API.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_

#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "url/gurl.h"

class ExtensionFunctionDispatcher;

namespace file_manager {

// Returns the ID of the tab associated with the dispatcher. Returns 0 on
// error.
int32 GetTabId(ExtensionFunctionDispatcher* dispatcher);

// Finds an icon in the list of icons. If unable to find an icon of the exact
// size requested, returns one with the next larger size. If all icons are
// smaller than the preferred size, we'll return the largest one available.
// Icons must be sorted by the icon size, smallest to largest. If there are no
// icons in the list, returns an empty URL.
GURL FindPreferredIcon(const google_apis::InstalledApp::IconList& icons,
                       int preferred_size);

// The preferred icon size, which should usually be used for FindPreferredIcon;
const int kPreferredIconSize = 16;

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_UTIL_H_
