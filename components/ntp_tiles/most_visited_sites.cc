// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/most_visited_sites.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#if defined(OS_ANDROID)
#include <jni.h>
#endif

#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/MostVisitedSites_jni.h"
#endif

using history::TopSites;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;

namespace ntp_tiles {

namespace {

// Identifiers for the various tile sources.
const char kHistogramClientName[] = "client";
const char kHistogramServerName[] = "server";
const char kHistogramPopularName[] = "popular";
const char kHistogramWhitelistName[] = "whitelist";

const base::Feature kDisplaySuggestionsServiceTiles{
    "DisplaySuggestionsServiceTiles", base::FEATURE_ENABLED_BY_DEFAULT};

// Log an event for a given |histogram| at a given element |position|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void LogHistogramEvent(const std::string& histogram,
                       int position,
                       int num_sites) {
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram,
      1,
      num_sites,
      num_sites + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->Add(position);
}

bool ShouldShowPopularSites() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kPopularSitesFieldTrialName);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableNTPPopularSites))
    return false;

  if (cmd_line->HasSwitch(switches::kEnableNTPPopularSites))
    return true;

#if defined(OS_ANDROID)
  if (Java_MostVisitedSites_isPopularSitesForceEnabled(
          base::android::AttachCurrentThread())) {
    return true;
  }
#endif

  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

// Determine whether we need any tiles from PopularSites to fill up a grid of
// |num_tiles| tiles.
bool NeedPopularSites(const PrefService* prefs, int num_tiles) {
  if (num_tiles <= prefs->GetInteger(prefs::kNumPersonalTiles))
    return false;

  // TODO(treib): Remove after M55.
  const base::ListValue* source_list =
      prefs->GetList(prefs::kDeprecatedNTPTilesIsPersonal);
  // If there aren't enough previous tiles to fill the grid, we need tiles from
  // PopularSites.
  if (static_cast<int>(source_list->GetSize()) < num_tiles)
    return true;
  // Otherwise, if any of the previous tiles are not personal, then also
  // get tiles from PopularSites.
  for (int i = 0; i < num_tiles; ++i) {
    bool is_personal = false;
    if (source_list->GetBoolean(i, &is_personal) && !is_personal)
      return true;
  }
  // The whole grid is already filled with personal tiles, no point in bothering
  // with popular ones.
  return false;
}

bool AreURLsEquivalent(const GURL& url1, const GURL& url2) {
  return url1.host() == url2.host() && url1.path() == url2.path();
}

std::string GetSourceHistogramName(NTPTileSource source) {
  switch (source) {
    case NTPTileSource::TOP_SITES:
      return kHistogramClientName;
    case NTPTileSource::POPULAR:
      return kHistogramPopularName;
    case NTPTileSource::WHITELIST:
      return kHistogramWhitelistName;
    case NTPTileSource::SUGGESTIONS_SERVICE:
      return kHistogramServerName;
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

MostVisitedSites::MostVisitedSites(PrefService* prefs,
                                   scoped_refptr<history::TopSites> top_sites,
                                   SuggestionsService* suggestions,
                                   std::unique_ptr<PopularSites> popular_sites,
                                   MostVisitedSitesSupervisor* supervisor)
    : prefs_(prefs),
      top_sites_(top_sites),
      suggestions_service_(suggestions),
      popular_sites_(std::move(popular_sites)),
      supervisor_(supervisor),
      observer_(nullptr),
      num_sites_(0),
      waiting_for_most_visited_sites_(true),
      waiting_for_popular_sites_(true),
      recorded_uma_(false),
      top_sites_observer_(this),
      mv_source_(NTPTileSource::SUGGESTIONS_SERVICE),
      weak_ptr_factory_(this) {
  DCHECK(prefs_);
  // top_sites_ can be null in tests.
  // TODO(sfiera): have iOS use a dummy TopSites in its tests.
  DCHECK(suggestions_service_);
  if (supervisor_)
    supervisor_->SetObserver(this);
}

MostVisitedSites::~MostVisitedSites() {
  if (supervisor_)
    supervisor_->SetObserver(nullptr);
}

void MostVisitedSites::SetMostVisitedURLsObserver(Observer* observer,
                                                  int num_sites) {
  DCHECK(observer);
  observer_ = observer;
  num_sites_ = num_sites;

  if (popular_sites_ && ShouldShowPopularSites() &&
      NeedPopularSites(prefs_, num_sites_)) {
    popular_sites_->StartFetch(
        false, base::Bind(&MostVisitedSites::OnPopularSitesAvailable,
                          base::Unretained(this)));
  } else {
    waiting_for_popular_sites_ = false;
  }

  if (top_sites_) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites_->SyncWithHistory();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    top_sites_observer_.Add(top_sites_.get());
  }

  suggestions_subscription_ = suggestions_service_->AddCallback(
      base::Bind(&MostVisitedSites::OnSuggestionsProfileAvailable,
                 base::Unretained(this)));

  // Immediately build the current set of tiles, getting suggestions from the
  // SuggestionsService's cache or, if that is empty, sites from TopSites.
  BuildCurrentTiles();
  // Also start a request for fresh suggestions.
  suggestions_service_->FetchSuggestionsData();
}

void MostVisitedSites::AddOrRemoveBlacklistedUrl(const GURL& url,
                                                 bool add_url) {
  if (top_sites_) {
    // Always blacklist in the local TopSites.
    if (add_url)
      top_sites_->AddBlacklistedURL(url);
    else
      top_sites_->RemoveBlacklistedURL(url);
  }

  // Only blacklist in the server-side suggestions service if it's active.
  if (mv_source_ == NTPTileSource::SUGGESTIONS_SERVICE) {
    if (add_url)
      suggestions_service_->BlacklistURL(url);
    else
      suggestions_service_->UndoBlacklistURL(url);
  }
}

void MostVisitedSites::RecordTileTypeMetrics(
    const std::vector<MostVisitedTileType>& tile_types,
    const std::vector<NTPTileSource>& sources) {
  int counts_per_type[NUM_TILE_TYPES] = {0};
  for (size_t i = 0; i < tile_types.size(); ++i) {
    MostVisitedTileType tile_type = tile_types[i];
    ++counts_per_type[tile_type];

    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileType", tile_type, NUM_TILE_TYPES);

    std::string histogram = base::StringPrintf(
        "NewTabPage.TileType.%s",
        GetSourceHistogramName(sources[i]).c_str());
    LogHistogramEvent(histogram, tile_type, NUM_TILE_TYPES);
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsReal",
                              counts_per_type[ICON_REAL]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsColor",
                              counts_per_type[ICON_COLOR]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsGray",
                              counts_per_type[ICON_DEFAULT]);
}

void MostVisitedSites::RecordOpenedMostVisitedItem(
    int index,
    MostVisitedTileType tile_type,
    NTPTileSource source) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisited", index, num_sites_);

  std::string histogram = base::StringPrintf(
      "NewTabPage.MostVisited.%s", GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, index, num_sites_);

  UMA_HISTOGRAM_ENUMERATION(
      "NewTabPage.TileTypeClicked", tile_type, NUM_TILE_TYPES);

  histogram = base::StringPrintf("NewTabPage.TileTypeClicked.%s",
                                 GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, tile_type, NUM_TILE_TYPES);
}

void MostVisitedSites::OnBlockedSitesChanged() {
  BuildCurrentTiles();
}

// static
void MostVisitedSites::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNumPersonalTiles, 0);
  // TODO(treib): Remove after M55.
  registry->RegisterListPref(prefs::kDeprecatedNTPTilesURL);
  registry->RegisterListPref(prefs::kDeprecatedNTPTilesIsPersonal);
}

void MostVisitedSites::BuildCurrentTiles() {
  // Get the current suggestions from cache. If the cache is empty, this will
  // fall back to TopSites.
  OnSuggestionsProfileAvailable(
      suggestions_service_->GetSuggestionsDataFromCache());
}

void MostVisitedSites::InitiateTopSitesQuery() {
  if (!top_sites_)
    return;
  top_sites_->GetMostVisitedURLs(
      base::Bind(&MostVisitedSites::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      false);
}

base::FilePath MostVisitedSites::GetWhitelistLargeIconPath(const GURL& url) {
  if (supervisor_) {
    for (const auto& whitelist : supervisor_->whitelists()) {
      if (AreURLsEquivalent(whitelist.entry_point, url))
        return whitelist.large_icon_path;
    }
  }
  return base::FilePath();
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& visited_list) {
  NTPTilesVector tiles;
  size_t num_tiles =
      std::min(visited_list.size(), static_cast<size_t>(num_sites_));
  for (size_t i = 0; i < num_tiles; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.is_empty()) {
      num_tiles = i;
      break;  // This is the signal that there are no more real visited sites.
    }
    if (supervisor_ && supervisor_->IsBlocked(visited.url))
      continue;

    NTPTile tile;
    tile.title = visited.title;
    tile.url = visited.url;
    tile.source = NTPTileSource::TOP_SITES;
    tile.whitelist_icon_path = GetWhitelistLargeIconPath(visited.url);

    tiles.push_back(std::move(tile));
  }

  waiting_for_most_visited_sites_ = false;
  mv_source_ = NTPTileSource::TOP_SITES;
  SaveNewTiles(std::move(tiles));
  NotifyMostVisitedURLsObserver();
}

void MostVisitedSites::OnSuggestionsProfileAvailable(
    const SuggestionsProfile& suggestions_profile) {
  int num_tiles = suggestions_profile.suggestions_size();
  // With no server suggestions, fall back to local TopSites.
  if (num_tiles == 0 ||
      !base::FeatureList::IsEnabled(kDisplaySuggestionsServiceTiles)) {
    InitiateTopSitesQuery();
    return;
  }
  if (num_sites_ < num_tiles)
    num_tiles = num_sites_;

  NTPTilesVector tiles;
  for (int i = 0; i < num_tiles; ++i) {
    const ChromeSuggestion& suggestion_pb = suggestions_profile.suggestions(i);
    GURL url(suggestion_pb.url());
    if (supervisor_ && supervisor_->IsBlocked(url))
      continue;

    NTPTile tile;
    tile.title = base::UTF8ToUTF16(suggestion_pb.title());
    tile.url = url;
    tile.source = NTPTileSource::SUGGESTIONS_SERVICE;
    tile.whitelist_icon_path = GetWhitelistLargeIconPath(url);

    tiles.push_back(std::move(tile));
  }

  waiting_for_most_visited_sites_ = false;
  mv_source_ = NTPTileSource::SUGGESTIONS_SERVICE;
  SaveNewTiles(std::move(tiles));
  NotifyMostVisitedURLsObserver();
}

NTPTilesVector MostVisitedSites::CreateWhitelistEntryPointTiles(
    const NTPTilesVector& personal_tiles) {
  if (!supervisor_) {
    return NTPTilesVector();
  }

  size_t num_personal_tiles = personal_tiles.size();
  DCHECK_LE(num_personal_tiles, static_cast<size_t>(num_sites_));

  size_t num_whitelist_tiles = num_sites_ - num_personal_tiles;
  NTPTilesVector whitelist_tiles;

  std::set<std::string> personal_hosts;
  for (const auto& tile : personal_tiles)
    personal_hosts.insert(tile.url.host());

  for (const auto& whitelist : supervisor_->whitelists()) {
    // Skip blacklisted sites.
    if (top_sites_ && top_sites_->IsBlacklisted(whitelist.entry_point))
      continue;

    // Skip tiles already present.
    if (personal_hosts.find(whitelist.entry_point.host()) !=
        personal_hosts.end())
      continue;

    // Skip whitelist entry points that are manually blocked.
    if (supervisor_->IsBlocked(whitelist.entry_point))
      continue;

    NTPTile tile;
    tile.title = whitelist.title;
    tile.url = whitelist.entry_point;
    tile.source = NTPTileSource::WHITELIST;
    tile.whitelist_icon_path = whitelist.large_icon_path;

    whitelist_tiles.push_back(std::move(tile));
    if (whitelist_tiles.size() >= num_whitelist_tiles)
      break;
  }

  return whitelist_tiles;
}

NTPTilesVector MostVisitedSites::CreatePopularSitesTiles(
    const NTPTilesVector& personal_tiles,
    const NTPTilesVector& whitelist_tiles) {
  // For child accounts popular sites tiles will not be added.
  if (supervisor_ && supervisor_->IsChildProfile())
    return NTPTilesVector();

  size_t num_tiles = personal_tiles.size() + whitelist_tiles.size();
  DCHECK_LE(num_tiles, static_cast<size_t>(num_sites_));

  // Collect non-blacklisted popular suggestions, skipping those already present
  // in the personal suggestions.
  size_t num_popular_sites_tiles = num_sites_ - num_tiles;
  NTPTilesVector popular_sites_tiles;

  if (num_popular_sites_tiles > 0 && popular_sites_) {
    std::set<std::string> hosts;
    for (const auto& tile : personal_tiles)
      hosts.insert(tile.url.host());
    for (const auto& tile : whitelist_tiles)
      hosts.insert(tile.url.host());
    for (const PopularSites::Site& popular_site : popular_sites_->sites()) {
      // Skip blacklisted sites.
      if (top_sites_ && top_sites_->IsBlacklisted(popular_site.url))
        continue;
      std::string host = popular_site.url.host();
      // Skip tiles already present in personal or whitelists.
      if (hosts.find(host) != hosts.end())
        continue;

      NTPTile tile;
      tile.title = popular_site.title;
      tile.url = GURL(popular_site.url);
      tile.source = NTPTileSource::POPULAR;

      popular_sites_tiles.push_back(std::move(tile));
      if (popular_sites_tiles.size() >= num_popular_sites_tiles)
        break;
    }
  }
  return popular_sites_tiles;
}

void MostVisitedSites::SaveNewTiles(NTPTilesVector personal_tiles) {
  NTPTilesVector whitelist_tiles =
      CreateWhitelistEntryPointTiles(personal_tiles);
  NTPTilesVector popular_sites_tiles =
      CreatePopularSitesTiles(personal_tiles, whitelist_tiles);

  size_t num_actual_tiles = personal_tiles.size() + whitelist_tiles.size() +
                            popular_sites_tiles.size();
  DCHECK_LE(num_actual_tiles, static_cast<size_t>(num_sites_));

  current_tiles_ =
      MergeTiles(std::move(personal_tiles), std::move(whitelist_tiles),
                 std::move(popular_sites_tiles));
  DCHECK_EQ(num_actual_tiles, current_tiles_.size());

  int num_personal_tiles = 0;
  for (const auto& tile : current_tiles_) {
    if (tile.source != NTPTileSource::POPULAR)
      num_personal_tiles++;
  }
  prefs_->SetInteger(prefs::kNumPersonalTiles, num_personal_tiles);
  // TODO(treib): Remove after M55.
  prefs_->ClearPref(prefs::kDeprecatedNTPTilesIsPersonal);
  prefs_->ClearPref(prefs::kDeprecatedNTPTilesURL);
}

// static
NTPTilesVector MostVisitedSites::MergeTiles(NTPTilesVector personal_tiles,
                                            NTPTilesVector whitelist_tiles,
                                            NTPTilesVector popular_tiles) {
  NTPTilesVector merged_tiles;
  std::move(personal_tiles.begin(), personal_tiles.end(),
            std::back_inserter(merged_tiles));
  std::move(whitelist_tiles.begin(), whitelist_tiles.end(),
            std::back_inserter(merged_tiles));
  std::move(popular_tiles.begin(), popular_tiles.end(),
            std::back_inserter(merged_tiles));
  return merged_tiles;
}

void MostVisitedSites::NotifyMostVisitedURLsObserver() {
  if (!waiting_for_most_visited_sites_ && !waiting_for_popular_sites_ &&
      !recorded_uma_) {
    RecordImpressionUMAMetrics();
    recorded_uma_ = true;
  }

  if (!observer_)
    return;

  observer_->OnMostVisitedURLsAvailable(current_tiles_);
}

void MostVisitedSites::OnPopularSitesAvailable(bool success) {
  waiting_for_popular_sites_ = false;

  if (!success) {
    LOG(WARNING) << "Download of popular sites failed";
    return;
  }

  // Pass the popular sites to the observer. This will cause it to fetch any
  // missing icons, but will *not* cause it to display the popular sites.
  observer_->OnPopularURLsAvailable(popular_sites_->sites());

  // Re-build the tile list. Once done, this will notify the observer.
  BuildCurrentTiles();
}

void MostVisitedSites::RecordImpressionUMAMetrics() {
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.NumberOfTiles",
                              current_tiles_.size());

  for (size_t i = 0; i < current_tiles_.size(); i++) {
    UMA_HISTOGRAM_ENUMERATION(
        "NewTabPage.SuggestionsImpression", static_cast<int>(i), num_sites_);

    std::string histogram = base::StringPrintf(
        "NewTabPage.SuggestionsImpression.%s",
        GetSourceHistogramName(current_tiles_[i].source).c_str());
    LogHistogramEvent(histogram, static_cast<int>(i), num_sites_);
  }
}

void MostVisitedSites::TopSitesLoaded(TopSites* top_sites) {}

void MostVisitedSites::TopSitesChanged(TopSites* top_sites,
                                       ChangeReason change_reason) {
  if (mv_source_ == NTPTileSource::TOP_SITES) {
    // The displayed tiles are invalidated.
    InitiateTopSitesQuery();
  }
}

}  // namespace ntp_tiles
