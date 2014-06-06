// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_
#define CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_

#include <string>

#include "extensions/common/extension.h"

class PrefService;

// States for bookmark experiment. They are set by Chrome sync into
// sync_driver::prefs::kEnhancedBookmarksExperimentEnabled user preference and
// used for UMA reporting as well.
enum BookmarksExperimentState {
  BOOKMARKS_EXPERIMENT_NONE,
  BOOKMARKS_EXPERIMENT_ENABLED,
  BOOKMARKS_EXPERIMENT_ENABLED_USER_OPT_OUT,
  BOOKMARKS_EXPERIMENT_ENABLED_FROM_FINCH,
  BOOKMARKS_EXPERIMENT_OPT_OUT_FROM_FINCH,
  BOOKMARKS_EXPERIMENT_ENABLED_FROM_FINCH_USER_SIGNEDIN,
  BOOKMARKS_EXPERIMENT_ENABLED_FROM_SYNC_UNKNOWN,
  BOOKMARKS_EXPERIMENT_ENUM_SIZE
};

// Returns true and sets |extension_id| if bookmarks experiment enabled
//         false if no bookmark experiment or extension id is empty.
bool GetBookmarksExperimentExtensionID(const PrefService* user_prefs,
                                       std::string* extension_id);

// Updates bookmark experiment state based on information from Chrome sync
// and Finch experiments.
void UpdateBookmarksExperimentState(
    PrefService* user_prefs,
    PrefService* local_state,
    bool user_signed_in,
    BookmarksExperimentState experiment_enabled_from_sync);

// Sets flag to opt-in user into Finch experiment.
void ForceFinchBookmarkExperimentIfNeeded(
    PrefService* local_state,
    BookmarksExperimentState bookmarks_experiment_state);

// Returns true if enhanced bookmarks experiment is enabled.
// Experiment could be enable from Chrome sync or from Finch.
bool IsEnhancedBookmarksExperimentEnabled();

#if defined(OS_ANDROID)
bool IsEnhancedBookmarkImageFetchingEnabled();
#endif

// Returns true when flag enable-dom-distiller is set or enabled from Finch.
bool IsEnableDomDistillerSet();

// Returns true when flag enable-sync-articles is set or enabled from Finch.
bool IsEnableSyncArticlesSet();

#endif  // CHROME_BROWSER_BOOKMARKS_ENHANCED_BOOKMARKS_FEATURES_H_
