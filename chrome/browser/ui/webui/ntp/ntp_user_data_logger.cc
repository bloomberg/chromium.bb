// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/search_urls.h"
#include "chrome/common/url_constants.h"
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

  // Only log the following statistics if at least one tile is recorded. This
  // check is required because the statistics are emitted whenever the user
  // changes tab away from the NTP. However, if the user comes back to that NTP
  // later the statistics are not regenerated (i.e. they are all 0). If we log
  // them again we get a strong bias.
  if (number_of_tiles_ > 0) {
    UMA_HISTOGRAM_ENUMERATION(
        "NewTabPage.SuggestionsType",
        has_server_side_suggestions_ ? SERVER_SIDE : CLIENT_SIDE,
        SUGGESTIONS_TYPE_COUNT);
    has_server_side_suggestions_ = false;
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
  }
}

void NTPUserDataLogger::LogEvent(NTPLoggingEventType event) {
  switch (event) {
    case NTP_SERVER_SIDE_SUGGESTION:
      has_server_side_suggestions_ = true;
      break;
    case NTP_CLIENT_SIDE_SUGGESTION:
      // We should never get a mix of server and client side suggestions,
      // otherwise there could be a race condition depending on the order in
      // which the iframes call this method.
      DCHECK(!has_server_side_suggestions_);
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
      number_of_tiles_(0),
      number_of_thumbnail_tiles_(0),
      number_of_gray_tiles_(0),
      number_of_external_tiles_(0),
      number_of_thumbnail_errors_(0),
      number_of_gray_tile_fallbacks_(0),
      number_of_external_tile_fallbacks_(0),
      number_of_mouseovers_(0) {
}
