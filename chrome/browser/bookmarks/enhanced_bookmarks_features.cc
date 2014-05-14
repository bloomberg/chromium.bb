// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace {

const char kFieldTrialName[] = "EnhancedBookmarks";

};  // namespace

void UpdateBookmarksExperiment(
    PrefService* local_state,
    BookmarksExperimentState bookmarks_experiment_state) {
  ListPrefUpdate update(local_state, prefs::kEnabledLabsExperiments);
  base::ListValue* experiments_list = update.Get();
  size_t index;
  if (bookmarks_experiment_state == kNoBookmarksExperiment) {
    experiments_list->Remove(
        base::StringValue(switches::kManualEnhancedBookmarks), &index);
    experiments_list->Remove(
        base::StringValue(switches::kManualEnhancedBookmarksOptout), &index);
  } else if (bookmarks_experiment_state == kBookmarksExperimentEnabled) {
    experiments_list->Remove(
        base::StringValue(switches::kManualEnhancedBookmarksOptout), &index);
    experiments_list->AppendIfNotPresent(
        new base::StringValue(switches::kManualEnhancedBookmarks));
  } else if (bookmarks_experiment_state ==
                 kBookmarksExperimentEnabledUserOptOut) {
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

  std::string ext_id = GetEnhancedBookmarksExtensionIdFromFinch();
  extensions::FeatureProvider* feature_provider =
      extensions::FeatureProvider::GetPermissionFeatures();
  extensions::Feature* feature = feature_provider->GetFeature("metricsPrivate");
  return feature && feature->IsIdInWhitelist(ext_id);
}

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

std::string GetEnhancedBookmarksExtensionIdFromFinch() {
  return chrome_variations::GetVariationParamValue(kFieldTrialName, "id");
}
