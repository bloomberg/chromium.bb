// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_metrics.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

// Macro to log UMA statistics related to the 8 tiles shown on the NTP.
#define UMA_HISTOGRAM_NTP_TILES(name, sample) \
    UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 0, 8, 9)

namespace {

// Used to track if suggestions were issued by the client or the server.
enum SuggestionsType {
  CLIENT_SIDE = 0,
  SERVER_SIDE = 1,
  SUGGESTIONS_TYPE_COUNT = 2
};

// Number of Most Visited elements on the NTP for logging purposes.
const int kNumMostVisited = 8;

// Name of the histogram keeping track of Most Visited impressions.
const char kMostVisitedImpressionHistogramName[] =
    "NewTabPage.SuggestionsImpression";

// Format string to generate the name for the histogram keeping track of
// suggestion impressions.
const char kMostVisitedImpressionHistogramWithProvider[] =
    "NewTabPage.SuggestionsImpression.%s";

// Name of the histogram keeping track of Most Visited navigations.
const char kMostVisitedNavigationHistogramName[] =
    "NewTabPage.MostVisited";

// Format string to generate the name for the histogram keeping track of
// suggestion navigations.
const char kMostVisitedNavigationHistogramWithProvider[] =
    "NewTabPage.MostVisited.%s";

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NTPUserDataLogger);


// Log a time event for a given |histogram| at a given |value|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void logLoadTimeHistogram(const std::string& histogram, base::TimeDelta value) {
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(60), 100,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(value);
}


NTPUserDataLogger::~NTPUserDataLogger() {}

// static
NTPUserDataLogger* NTPUserDataLogger::GetOrCreateFromWebContents(
      content::WebContents* content) {
  // Calling CreateForWebContents when an instance is already attached has no
  // effect, so we can do this.
  NTPUserDataLogger::CreateForWebContents(content);
  NTPUserDataLogger* logger = NTPUserDataLogger::FromWebContents(content);

  // We record the URL of this NTP in order to identify navigations that
  // originate from it. We use the NavigationController's URL since it might
  // differ from the WebContents URL which is usually chrome://newtab/.
  const content::NavigationEntry* entry =
      content->GetController().GetVisibleEntry();
  if (entry)
    logger->ntp_url_ = entry->GetURL();

  return logger;
}

// static
std::string NTPUserDataLogger::GetMostVisitedImpressionHistogramNameForProvider(
    const std::string& provider) {
  return base::StringPrintf(kMostVisitedImpressionHistogramWithProvider,
                            provider.c_str());
}

// static
std::string NTPUserDataLogger::GetMostVisitedNavigationHistogramNameForProvider(
    const std::string& provider) {
  return base::StringPrintf(kMostVisitedNavigationHistogramWithProvider,
                            provider.c_str());
}

void NTPUserDataLogger::EmitNtpStatistics() {
  UMA_HISTOGRAM_COUNTS("NewTabPage.NumberOfMouseOvers", number_of_mouseovers_);
  number_of_mouseovers_ = 0;

  // We only send statistics once per page.
  // And we don't send if there are no tiles recorded.
  if (has_emitted_ || !number_of_tiles_)
    return;

  // LoadTime only gets update once per page, so we don't have it on reloads.
  if (load_time_ > base::TimeDelta::FromMilliseconds(0)) {
    logLoadTimeHistogram("NewTabPage.LoadTime", load_time_);

    // Split between ML and MV.
    std::string type = has_server_side_suggestions_ ?
        "MostLikely" : "MostVisited";
    logLoadTimeHistogram("NewTabPage.LoadTime." + type, load_time_);
    // Split between Web and Local.
    std::string source = ntp_url_.SchemeIsHTTPOrHTTPS() ? "Web" : "LocalNTP";
    logLoadTimeHistogram("NewTabPage.LoadTime." + source, load_time_);

    // Split between Startup and non-startup.
    std::string status = during_startup_ ? "Startup" : "NewTab";
    logLoadTimeHistogram("NewTabPage.LoadTime." + status, load_time_);

    load_time_ = base::TimeDelta::FromMilliseconds(0);
  }
  UMA_HISTOGRAM_ENUMERATION(
      "NewTabPage.SuggestionsType",
      has_server_side_suggestions_ ? SERVER_SIDE : CLIENT_SIDE,
      SUGGESTIONS_TYPE_COUNT);
  has_server_side_suggestions_ = false;
  has_client_side_suggestions_ = false;
  UMA_HISTOGRAM_NTP_TILES("NewTabPage.NumberOfTiles", number_of_tiles_);
  number_of_tiles_ = 0;
  UMA_HISTOGRAM_NTP_TILES("NewTabPage.NumberOfThumbnailTiles",
                          number_of_thumbnail_tiles_);
  number_of_thumbnail_tiles_ = 0;
  UMA_HISTOGRAM_NTP_TILES("NewTabPage.NumberOfGrayTiles",
                          number_of_gray_tiles_);
  number_of_gray_tiles_ = 0;
  UMA_HISTOGRAM_NTP_TILES("NewTabPage.NumberOfExternalTiles",
                          number_of_external_tiles_);
  number_of_external_tiles_ = 0;
  UMA_HISTOGRAM_NTP_TILES("NewTabPage.NumberOfThumbnailErrors",
                          number_of_thumbnail_errors_);
  number_of_thumbnail_errors_ = 0;
  UMA_HISTOGRAM_NTP_TILES("NewTabPage.NumberOfGrayTileFallbacks",
                          number_of_gray_tile_fallbacks_);
  number_of_gray_tile_fallbacks_ = 0;
  UMA_HISTOGRAM_NTP_TILES("NewTabPage.NumberOfExternalTileFallbacks",
                          number_of_external_tile_fallbacks_);
  number_of_external_tile_fallbacks_ = 0;
  has_emitted_ = true;
  during_startup_ = false;
}

void NTPUserDataLogger::LogEvent(NTPLoggingEventType event,
                                 base::TimeDelta time) {
  switch (event) {
    // It is possible that our page gets update with a different set of
    // suggestions if the NTP is left open enough time.
    // In either case, we want to flush our stats before recounting again.
    case NTP_SERVER_SIDE_SUGGESTION:
      if (has_client_side_suggestions_)
        EmitNtpStatistics();
      has_server_side_suggestions_ = true;
      break;
    case NTP_CLIENT_SIDE_SUGGESTION:
      if (has_server_side_suggestions_)
        EmitNtpStatistics();
      has_client_side_suggestions_ = true;
      break;
    case NTP_TILE:
      number_of_tiles_++;
      break;
    case NTP_THUMBNAIL_TILE:
      number_of_thumbnail_tiles_++;
      break;
    case NTP_GRAY_TILE:
      number_of_gray_tiles_++;
      break;
    case NTP_EXTERNAL_TILE:
      number_of_external_tiles_++;
      break;
    case NTP_THUMBNAIL_ERROR:
      number_of_thumbnail_errors_++;
      break;
    case NTP_GRAY_TILE_FALLBACK:
      number_of_gray_tile_fallbacks_++;
      break;
    case NTP_EXTERNAL_TILE_FALLBACK:
      number_of_external_tile_fallbacks_++;
      break;
    case NTP_MOUSEOVER:
      number_of_mouseovers_++;
      break;
    case NTP_TILE_LOADED:
      // The time at which the last tile has loaded (title, thumbnail or single)
      // is a good proxy for the total load time of the NTP, therefore we keep
      // the max as the load time.
      load_time_ = std::max(load_time_, time);
      break;
    default:
      NOTREACHED();
  }
}

void NTPUserDataLogger::LogMostVisitedImpression(
    int position, const base::string16& provider) {
  // Log the Most Visited navigation for navigations that have providers and
  // those that dont.
  UMA_HISTOGRAM_ENUMERATION(kMostVisitedImpressionHistogramName, position,
                            kNumMostVisited);

  // If a provider is specified, log the metric specific to it.
  if (!provider.empty()) {
    // Cannot rely on UMA histograms macro because the name of the histogram is
    // generated dynamically.
    base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
        GetMostVisitedImpressionHistogramNameForProvider(
            base::UTF16ToUTF8(provider)),
        1,
        kNumMostVisited,
        kNumMostVisited + 1,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(position);
  }
}

void NTPUserDataLogger::LogMostVisitedNavigation(
    int position, const base::string16& provider) {
  // Log the Most Visited navigation for navigations that have providers and
  // those that dont.
  UMA_HISTOGRAM_ENUMERATION(kMostVisitedNavigationHistogramName, position,
                            kNumMostVisited);

  // If a provider is specified, log the metric specific to it.
  if (!provider.empty()) {
    // Cannot rely on UMA histograms macro because the name of the histogram is
    // generated dynamically.
    base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
        GetMostVisitedNavigationHistogramNameForProvider(
            base::UTF16ToUTF8(provider)),
        1,
        kNumMostVisited,
        kNumMostVisited + 1,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(position);
  }

  // Records the action. This will be available as a time-stamped stream
  // server-side and can be used to compute time-to-long-dwell.
  content::RecordAction(base::UserMetricsAction("MostVisited_Clicked"));
}

// content::WebContentsObserver override
void NTPUserDataLogger::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.previous_url.is_valid())
    return;

  if (search::MatchesOriginAndPath(ntp_url_, load_details.previous_url)) {
    EmitNtpStatistics();
  }
}

NTPUserDataLogger::NTPUserDataLogger(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      has_server_side_suggestions_(false),
      has_client_side_suggestions_(false),
      number_of_tiles_(0),
      number_of_thumbnail_tiles_(0),
      number_of_gray_tiles_(0),
      number_of_external_tiles_(0),
      number_of_thumbnail_errors_(0),
      number_of_gray_tile_fallbacks_(0),
      number_of_external_tile_fallbacks_(0),
      number_of_mouseovers_(0),
      has_emitted_(false),
      during_startup_(false) {
  during_startup_ = !AfterStartupTaskUtils::IsBrowserStartupComplete();

  // We record metrics about session data here because when this class typically
  // emits metrics it is too late. This session data would theoretically have
  // been used to populate the page, and we want to learn about its state when
  // the NTP is being generated.
  if (contents) {
    ProfileSyncService* sync = ProfileSyncServiceFactory::GetForProfile(
        Profile::FromBrowserContext(contents->GetBrowserContext()));
    if (sync) {
      browser_sync::SessionsSyncManager* sessions =
          static_cast<browser_sync::SessionsSyncManager*>(
              sync->GetSessionsSyncableService());
      if (sessions) {
        sync_sessions::SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
            sessions);
      }
    }
  }
}
