// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_
#define CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_

#include <string>

#include "build/build_config.h"

class PrefService;

#if defined(OS_ANDROID)
// Returns true if enhanced bookmark salient image prefetching is enabled.
// This can be controlled by field trial.
bool IsEnhancedBookmarkImageFetchingEnabled(const PrefService* user_prefs);

// Returns true if enhanced bookmarks is enabled.
bool IsEnhancedBookmarksEnabled();
#endif

// Returns true and sets |extension_id| if enhanced bookmarks experiment is
// enabled. Returns false if no bookmark experiment or extension id is empty.
// Besides, this method takes commandline flags into account before trying
// to get an extension_id.
bool IsEnhancedBookmarksEnabled(std::string* extension_id);

// Returns true when flag enable-dom-distiller is set or enabled from Finch.
bool IsEnableDomDistillerSet();

// Returns true when flag enable-sync-articles is set or enabled from Finch.
bool IsEnableSyncArticlesSet();

#endif  // CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_
