// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/most_visited_tiles_experiment.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "chrome/common/instant_types.h"

namespace history {

namespace {

// Constants for the most visited tile placement field trial.
// ex:
// "OneEightGroup_Flipped" --> Will cause tile 1 and 8 to be flipped.
// "OneEightGroup_NoChange" --> Will not flip anything.
//
// See field trial config (MostVisitedTilePlacement.json) for details.
const char kMostVisitedFieldTrialName[] = "MostVisitedTilePlacement";
// Name of histogram tracking types of actions carried out by the field trial.
const char kMostVisitedExperimentHistogramName[] =
    "NewTabPage.MostVisitedTilePlacementExperiment";
const char kOneEightGroupPrefix[] = "OneEight";
const char kOneFourGroupPrefix[] = "OneFour";
const char kFlippedSuffix[] = "Flipped";
const char kDontShowOpenURLsGroupName[] = "DontShowOpenTabs";
// Minimum number of Most Visited suggestions required in order for the Most
// Visited Field Trial to remove a URL already open in the browser.
const size_t kMinUrlSuggestions = 8;

}  // namespace

// static
void MostVisitedTilesExperiment::MaybeShuffle(MostVisitedURLList* data) {
  const std::string group_name =
      base::FieldTrialList::FindFullName(kMostVisitedFieldTrialName);

  // Depending on the study group of the client, we might flip the 1st and 4th
  // tiles, or the 1st and 8th, or do nothing.
  if (!EndsWith(group_name, kFlippedSuffix, true))
    return;

  size_t index_to_flip = 0;
  if (StartsWithASCII(group_name, kOneEightGroupPrefix, true)) {
    if (data->size() < 8) {
      LogInHistogram(NTP_TILE_EXPERIMENT_ACTION_TOO_FEW_URLS_TILES_1_8);
      return;
    }
    index_to_flip = 7;
  } else if (StartsWithASCII(group_name, kOneFourGroupPrefix, true)) {
    if (data->size() < 4) {
      LogInHistogram(NTP_TILE_EXPERIMENT_ACTION_TOO_FEW_URLS_TILES_1_4);
      return;
    }
    index_to_flip = 3;
  }
  std::swap((*data)[0], (*data)[index_to_flip]);
}

// static
bool MostVisitedTilesExperiment::IsDontShowOpenURLsEnabled() {
  return base::FieldTrialList::FindFullName(kMostVisitedFieldTrialName) ==
      kDontShowOpenURLsGroupName;
}

// static
void MostVisitedTilesExperiment::RemoveItemsMatchingOpenTabs(
    const std::set<std::string>& open_urls,
    std::vector<InstantMostVisitedItem>* items) {
  for (size_t i = 0; i < items->size(); ) {
    const std::string& url = (*items)[i].url.spec();
    if (ShouldRemoveURL(open_urls, url, items->size()))
      items->erase(items->begin() + i);
    else
      ++i;
  }
}

// static
void MostVisitedTilesExperiment::RemovePageValuesMatchingOpenTabs(
    const std::set<std::string>& open_urls,
    base::ListValue* pages_value) {
  for (size_t i = 0; i < pages_value->GetSize(); ) {
    base::DictionaryValue* page_value;
    std::string url;
    if (pages_value->GetDictionary(i, &page_value) &&
        page_value->GetString("url", &url) &&
        ShouldRemoveURL(open_urls, url, pages_value->GetSize())) {
      pages_value->Remove(*page_value, &i);
    } else {
      ++i;
    }
  }
}

// static
void MostVisitedTilesExperiment::LogInHistogram(
    NtpTileExperimentActions action) {
  UMA_HISTOGRAM_ENUMERATION(kMostVisitedExperimentHistogramName,
                            action,
                            NUM_NTP_TILE_EXPERIMENT_ACTIONS);
}

// static
bool MostVisitedTilesExperiment::ShouldRemoveURL(
    const std::set<std::string>& open_urls,
    const std::string& url,
    const size_t size) {
  if (open_urls.count(url) == 0)
    return false;

  if (size <= kMinUrlSuggestions) {
    LogInHistogram(NTP_TILE_EXPERIMENT_ACTION_DID_NOT_REMOVE_URL);
    return false;
  }

  LogInHistogram(NTP_TILE_EXPERIMENT_ACTION_REMOVED_URL);
  return true;
}

}  // namespace history
