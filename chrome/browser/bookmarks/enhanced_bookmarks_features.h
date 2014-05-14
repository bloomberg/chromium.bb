// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_
#define CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_

#include <string>

#include "extensions/common/extension.h"

class PrefService;

enum BookmarksExperimentState {
  kNoBookmarksExperiment,
  kBookmarksExperimentEnabled,
  kBookmarksExperimentEnabledUserOptOut
};

// Sets flag to opt-in user into Finch experiment.
void UpdateBookmarksExperiment(
    PrefService* local_state,
    BookmarksExperimentState bookmarks_experiment_state);

// Returns true if enhanced bookmarks experiment is enabled.
bool IsEnhancedBookmarksExperimentEnabled();

// Returns true when flag enable-dom-distiller is set or enabled from Finch.
bool IsEnableDomDistillerSet();

// Returns true when flag enable-sync-articles is set or enabled from Finch.
bool IsEnableSyncArticlesSet();

// Get extension id from Finch EnhancedBookmarks group parameters.
std::string GetEnhancedBookmarksExtensionIdFromFinch();

#endif  // CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_
