// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature to automatically switch to the regular tabs panel in tab grid after
// closing the last incognito tab.
extern const base::Feature kClosingLastIncognitoTab;

// Feature to contain the NTP directly from browser container.
extern const base::Feature kBrowserContainerContainsNTP;

// Feature to show most visited sites and collection shortcuts in the omnibox
// popup instead of ZeroSuggest.
extern const base::Feature kOmniboxPopupShortcutIconsInZeroState;

// Feature to allow user to search for a copied image.
extern const base::Feature kSearchCopiedImage;

// Feature to take snapshots using |-drawViewHierarchy:|.
extern const base::Feature kSnapshotDrawView;

#endif  // IOS_CHROME_BROWSER_UI_UI_FEATURE_FLAGS_H_
