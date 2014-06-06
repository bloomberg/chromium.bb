// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/sync_driver/pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace {

const char kFieldTrialName[] = "EnhancedBookmarks";

// Get extension id from Finch EnhancedBookmarks group parameters.
std::string GetEnhancedBookmarksExtensionIdFromFinch() {
  return chrome_variations::GetVariationParamValue(kFieldTrialName, "id");
}

// Returns true if enhanced bookmarks experiment is enabled from Finch.
bool IsEnhancedBookmarksExperimentEnabledFromFinch() {
  std::string ext_id = GetEnhancedBookmarksExtensionIdFromFinch();
  const extensions::FeatureProvider* feature_provider =
      extensions::FeatureProvider::GetPermissionFeatures();
  extensions::Feature* feature = feature_provider->GetFeature("metricsPrivate");
  return feature && feature->IsIdInWhitelist(ext_id);
}

};  // namespace

bool GetBookmarksExperimentExtensionID(const PrefService* user_prefs,
                                       std::string* extension_id) {
  BookmarksExperimentState bookmarks_experiment_state =
      static_cast<BookmarksExperimentState>(user_prefs->GetInteger(
          sync_driver::prefs::kEnhancedBookmarksExperimentEnabled));
  if (bookmarks_experiment_state == BOOKMARKS_EXPERIMENT_ENABLED_FROM_FINCH) {
    *extension_id = GetEnhancedBookmarksExtensionIdFromFinch();
    return !extension_id->empty();
  }
  if (bookmarks_experiment_state == BOOKMARKS_EXPERIMENT_ENABLED) {
    *extension_id = user_prefs->GetString(
        sync_driver::prefs::kEnhancedBookmarksExtensionId);
    return !extension_id->empty();
  }

  return false;
}

void UpdateBookmarksExperimentState(
    PrefService* user_prefs,
    PrefService* local_state,
    bool user_signed_in,
    BookmarksExperimentState experiment_enabled_from_sync) {
 PrefService* flags_storage = local_state;
#if defined(OS_CHROMEOS)
  // Chrome OS is using user prefs for flags storage.
  flags_storage = user_prefs;
#endif

  BookmarksExperimentState bookmarks_experiment_state_before =
      static_cast<BookmarksExperimentState>(user_prefs->GetInteger(
          sync_driver::prefs::kEnhancedBookmarksExperimentEnabled));
  // If user signed out, clear possible previous state.
  if (!user_signed_in) {
    bookmarks_experiment_state_before = BOOKMARKS_EXPERIMENT_NONE;
    ForceFinchBookmarkExperimentIfNeeded(flags_storage,
        BOOKMARKS_EXPERIMENT_NONE);
  }

  // kEnhancedBookmarksExperiment flag could have values "", "1" and "0".
  // "0" - user opted out.
  bool opt_out = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                     switches::kEnhancedBookmarksExperiment) == "0";

  BookmarksExperimentState bookmarks_experiment_new_state =
      BOOKMARKS_EXPERIMENT_NONE;

  if (IsEnhancedBookmarksExperimentEnabledFromFinch() && !user_signed_in) {
    if (opt_out) {
      // Experiment enabled but user opted out.
      bookmarks_experiment_new_state = BOOKMARKS_EXPERIMENT_OPT_OUT_FROM_FINCH;
    } else {
      // Experiment enabled.
      bookmarks_experiment_new_state = BOOKMARKS_EXPERIMENT_ENABLED_FROM_FINCH;
    }
  } else if (experiment_enabled_from_sync == BOOKMARKS_EXPERIMENT_ENABLED) {
    // Experiment enabled from Chrome sync.
    if (opt_out) {
      // Experiment enabled but user opted out.
      bookmarks_experiment_new_state =
          BOOKMARKS_EXPERIMENT_ENABLED_USER_OPT_OUT;
    } else {
      // Experiment enabled.
      bookmarks_experiment_new_state = BOOKMARKS_EXPERIMENT_ENABLED;
    }
  } else if (experiment_enabled_from_sync == BOOKMARKS_EXPERIMENT_NONE) {
    // Experiment is not enabled from Chrome sync.
    bookmarks_experiment_new_state = BOOKMARKS_EXPERIMENT_NONE;
  } else if (bookmarks_experiment_state_before ==
             BOOKMARKS_EXPERIMENT_ENABLED) {
    if (opt_out) {
      // Experiment enabled but user opted out.
      bookmarks_experiment_new_state =
          BOOKMARKS_EXPERIMENT_ENABLED_USER_OPT_OUT;
    } else {
      bookmarks_experiment_new_state = BOOKMARKS_EXPERIMENT_ENABLED;
    }
  } else if (bookmarks_experiment_state_before ==
             BOOKMARKS_EXPERIMENT_ENABLED_USER_OPT_OUT) {
    if (opt_out) {
      bookmarks_experiment_new_state =
          BOOKMARKS_EXPERIMENT_ENABLED_USER_OPT_OUT;
    } else {
      // User opted in again.
      bookmarks_experiment_new_state = BOOKMARKS_EXPERIMENT_ENABLED;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("EnhancedBookmarks.SyncExperimentState",
                            bookmarks_experiment_new_state,
                            BOOKMARKS_EXPERIMENT_ENUM_SIZE);
  user_prefs->SetInteger(
      sync_driver::prefs::kEnhancedBookmarksExperimentEnabled,
      bookmarks_experiment_new_state);
  ForceFinchBookmarkExperimentIfNeeded(flags_storage,
                                       bookmarks_experiment_new_state);
}

void ForceFinchBookmarkExperimentIfNeeded(
    PrefService* flags_storage,
    BookmarksExperimentState bookmarks_experiment_state) {
  if (!flags_storage)
    return;
  ListPrefUpdate update(flags_storage, prefs::kEnabledLabsExperiments);
  base::ListValue* experiments_list = update.Get();
  if (!experiments_list)
    return;
  size_t index;
  if (bookmarks_experiment_state == BOOKMARKS_EXPERIMENT_NONE) {
    experiments_list->Remove(
        base::StringValue(switches::kManualEnhancedBookmarks), &index);
    experiments_list->Remove(
        base::StringValue(switches::kManualEnhancedBookmarksOptout), &index);
  } else if (bookmarks_experiment_state == BOOKMARKS_EXPERIMENT_ENABLED) {
    experiments_list->Remove(
        base::StringValue(switches::kManualEnhancedBookmarksOptout), &index);
    experiments_list->AppendIfNotPresent(
        new base::StringValue(switches::kManualEnhancedBookmarks));
  } else if (bookmarks_experiment_state ==
                 BOOKMARKS_EXPERIMENT_ENABLED_USER_OPT_OUT) {
    experiments_list->Remove(
        base::StringValue(switches::kManualEnhancedBookmarks), &index);
    experiments_list->AppendIfNotPresent(
        new base::StringValue(switches::kManualEnhancedBookmarksOptout));
  }
}

bool IsEnhancedBookmarksExperimentEnabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kManualEnhancedBookmarks) ||
      command_line->HasSwitch(switches::kManualEnhancedBookmarksOptout)) {
    return true;
  }

  return IsEnhancedBookmarksExperimentEnabledFromFinch();
}

#if defined(OS_ANDROID)
bool IsEnhancedBookmarkImageFetchingEnabled() {
  if (IsEnhancedBookmarksExperimentEnabled())
    return true;

  // Salient images are collected from visited bookmarked pages even if the
  // enhanced bookmark feature is turned off. This is to have some images
  // available so that in the future, when the feature is turned on, the user
  // experience is not a big list of flat colors. However as a precautionary
  // measure it is possible to disable this collection of images from finch.
  std::string disable_fetching = chrome_variations::GetVariationParamValue(
      kFieldTrialName, "DisableImagesFetching");
  return disable_fetching.empty();
}
#endif

bool IsEnableDomDistillerSet() {
  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableDomDistiller)) {
    return true;
  }
  if (chrome_variations::GetVariationParamValue(
          kFieldTrialName, "enable-dom-distiller") == "1")
    return true;

  return false;
}

bool IsEnableSyncArticlesSet() {
  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableSyncArticles)) {
    return true;
  }
  if (chrome_variations::GetVariationParamValue(
          kFieldTrialName, "enable-sync-articles") == "1")
    return true;

  return false;
}
