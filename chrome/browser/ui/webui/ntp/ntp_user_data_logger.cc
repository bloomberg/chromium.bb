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
#include "content/public/browser/web_contents.h"

namespace {

// Used to track if suggestions were issued by the client or the server.
enum SuggestionsType {
  CLIENT_SIDE = 0,
  SERVER_SIDE = 1,
  SUGGESTIONS_TYPE_COUNT = 2
};

// Format string to generate the name for the histogram keeping track of
// suggestion impressions.
const char kImpressionHistogramWithProvider[] =
    "NewTabPage.SuggestionsImpression.%s";

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

void NTPUserDataLogger::EmitThumbnailErrorRate() {
  DCHECK_LE(number_of_thumbnail_errors_, number_of_thumbnail_attempts_);
  if (number_of_thumbnail_attempts_ != 0) {
    UMA_HISTOGRAM_PERCENTAGE(
      "NewTabPage.ThumbnailErrorRate",
      GetPercentError(number_of_thumbnail_errors_,
                      number_of_thumbnail_attempts_));
  }
  DCHECK_LE(number_of_fallback_thumbnails_used_,
            number_of_fallback_thumbnails_requested_);
  if (number_of_fallback_thumbnails_requested_ != 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        "NewTabPage.ThumbnailFallbackRate",
        GetPercentError(number_of_fallback_thumbnails_used_,
                        number_of_fallback_thumbnails_requested_));
  }
  number_of_thumbnail_attempts_ = 0;
  number_of_thumbnail_errors_ = 0;
  number_of_fallback_thumbnails_requested_ = 0;
  number_of_fallback_thumbnails_used_ = 0;
}

void NTPUserDataLogger::EmitNtpStatistics() {
  UMA_HISTOGRAM_COUNTS("NewTabPage.NumberOfMouseOvers", number_of_mouseovers_);
  number_of_mouseovers_ = 0;
  UMA_HISTOGRAM_COUNTS("NewTabPage.NumberOfExternalTiles",
                       number_of_external_tiles_);
  number_of_external_tiles_ = 0;
  UMA_HISTOGRAM_ENUMERATION(
      "NewTabPage.SuggestionsType",
      server_side_suggestions_ ? SERVER_SIDE : CLIENT_SIDE,
      SUGGESTIONS_TYPE_COUNT);
  server_side_suggestions_ = false;
}

void NTPUserDataLogger::LogEvent(NTPLoggingEventType event) {
  switch (event) {
    case NTP_MOUSEOVER:
      number_of_mouseovers_++;
      break;
    case NTP_THUMBNAIL_ATTEMPT:
      number_of_thumbnail_attempts_++;
      break;
    case NTP_THUMBNAIL_ERROR:
      number_of_thumbnail_errors_++;
      break;
    case NTP_FALLBACK_THUMBNAIL_REQUESTED:
      number_of_fallback_thumbnails_requested_++;
      break;
    case NTP_FALLBACK_THUMBNAIL_USED:
      number_of_fallback_thumbnails_used_++;
      break;
    case NTP_SERVER_SIDE_SUGGESTION:
      server_side_suggestions_ = true;
      break;
    case NTP_CLIENT_SIDE_SUGGESTION:
      // We should never get a mix of server and client side suggestions,
      // otherwise there could be a race condition depending on the order in
      // which the iframes call this method.
      DCHECK(!server_side_suggestions_);
    break;
    case NTP_EXTERNAL_TILE:
      number_of_external_tiles_++;
      break;
    default:
      NOTREACHED();
  }
}

void NTPUserDataLogger::LogImpression(int position,
                                      const base::string16& provider) {
  // Cannot rely on UMA histograms macro because the name of the histogram is
  // generated dynamically.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      base::StringPrintf(kImpressionHistogramWithProvider,
                         UTF16ToUTF8(provider).c_str()),
      1, MostVisitedIframeSource::kNumMostVisited,
      MostVisitedIframeSource::kNumMostVisited + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(position);
}

// content::WebContentsObserver override
void NTPUserDataLogger::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.previous_url.is_valid())
    return;

  if (search::MatchesOriginAndPath(ntp_url_, load_details.previous_url)) {
    EmitNtpStatistics();
    // Only log thumbnail error rates for Instant NTP pages, as we do not have
    // this data for non-Instant NTPs.
    if (ntp_url_ != GURL(chrome::kChromeUINewTabURL))
      EmitThumbnailErrorRate();
  }
}

NTPUserDataLogger::NTPUserDataLogger(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      number_of_mouseovers_(0),
      number_of_thumbnail_attempts_(0),
      number_of_thumbnail_errors_(0),
      number_of_fallback_thumbnails_requested_(0),
      number_of_fallback_thumbnails_used_(0),
      number_of_external_tiles_(0),
      server_side_suggestions_(false) {
}

size_t NTPUserDataLogger::GetPercentError(size_t errors, size_t events) const {
  return (100 * errors) / events;
}
