// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_details.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NTPUserDataLogger);

NTPUserDataLogger::~NTPUserDataLogger() {}

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

void NTPUserDataLogger::EmitMouseoverCount() {
  UMA_HISTOGRAM_COUNTS("NewTabPage.NumberOfMouseOvers", number_of_mouseovers_);
  number_of_mouseovers_ = 0;
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
    default:
      NOTREACHED();
  }
}

// content::WebContentsObserver override
void NTPUserDataLogger::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.previous_url.is_valid())
    return;

  if (chrome::MatchesOriginAndPath(ntp_url_, load_details.previous_url)) {
    EmitMouseoverCount();
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
      number_of_fallback_thumbnails_used_(0) {
}

size_t NTPUserDataLogger::GetPercentError(size_t errors, size_t events) const {
  return (100 * errors) / events;
}
