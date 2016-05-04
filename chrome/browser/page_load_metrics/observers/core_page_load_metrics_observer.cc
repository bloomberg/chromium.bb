// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"

namespace {

// The number of buckets in the bitfield histogram. These buckets are described
// in rappor.xml in PageLoad.CoarseTiming.NavigationToFirstContentfulPaint.
// The bucket flag is defined by 1 << bucket_index, and is the bitfield
// representing which timing bucket the page load falls into, i.e. 000010
// would be the bucket flag showing that the page took between 2 and 4 seconds
// to load.
const size_t kNumRapporHistogramBuckets = 6;

uint64_t RapporHistogramBucketIndex(base::TimeDelta time) {
  int64_t seconds = time.InSeconds();
  if (seconds < 2)
    return 0;
  if (seconds < 4)
    return 1;
  if (seconds < 8)
    return 2;
  if (seconds < 16)
    return 3;
  if (seconds < 32)
    return 4;
  return 5;
}

}  // namespace

namespace internal {

const char kHistogramCommit[] = "PageLoad.Timing2.NavigationToCommit";
const char kHistogramFirstLayout[] = "PageLoad.Timing2.NavigationToFirstLayout";
const char kHistogramFirstTextPaint[] =
    "PageLoad.Timing2.NavigationToFirstTextPaint";
const char kHistogramDomContentLoaded[] =
    "PageLoad.Timing2.NavigationToDOMContentLoadedEventFired";
const char kHistogramDomLoadingToDomContentLoaded[] =
    "PageLoad.Timing2.DOMLoadingToDOMContentLoadedEventFired";
const char kHistogramLoad[] = "PageLoad.Timing2.NavigationToLoadEventFired";
const char kHistogramFirstPaint[] = "PageLoad.Timing2.NavigationToFirstPaint";
const char kHistogramFirstImagePaint[] =
    "PageLoad.Timing2.NavigationToFirstImagePaint";
const char kHistogramFirstContentfulPaint[] =
    "PageLoad.Timing2.NavigationToFirstContentfulPaint";
const char kHistogramDomLoadingToFirstContentfulPaint[] =
    "PageLoad.Timing2.DOMLoadingToFirstContentfulPaint";
const char kHistogramParseDuration[] = "PageLoad.Timing2.ParseDuration";
const char kHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.Timing2.ParseBlockedOnScriptLoad";
const char kHistogramParseBlockedOnScriptLoadParseComplete[] =
    "PageLoad.Timing2.ParseBlockedOnScriptLoad.ParseComplete";
const char kHistogramParseStartToFirstContentfulPaint[] =
    "PageLoad.Timing2.ParseStartToFirstContentfulPaint";
const char kHistogramParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Timing2.ParseBlockedOnScriptLoadFromDocumentWrite";
const char kHistogramParseBlockedOnScriptLoadDocumentWriteParseComplete[] =
    "PageLoad.Timing2.ParseBlockedOnScriptLoadFromDocumentWrite.ParseComplete";

const char kBackgroundHistogramCommit[] =
    "PageLoad.Timing2.NavigationToCommit.Background";
const char kBackgroundHistogramFirstLayout[] =
    "PageLoad.Timing2.NavigationToFirstLayout.Background";
const char kBackgroundHistogramFirstTextPaint[] =
    "PageLoad.Timing2.NavigationToFirstTextPaint.Background";
const char kBackgroundHistogramDomContentLoaded[] =
    "PageLoad.Timing2.NavigationToDOMContentLoadedEventFired.Background";
const char kBackgroundHistogramLoad[] =
    "PageLoad.Timing2.NavigationToLoadEventFired.Background";
const char kBackgroundHistogramFirstPaint[] =
    "PageLoad.Timing2.NavigationToFirstPaint.Background";
const char kBackgroundHistogramFirstImagePaint[] =
    "PageLoad.Timing2.NavigationToFirstImagePaint.Background.";
const char kBackgroundHistogramFirstContentfulPaint[] =
    "PageLoad.Timing2.NavigationToFirstContentfulPaint.Background";
const char kBackgroundHistogramParseDuration[] =
    "PageLoad.Timing2.ParseDuration.Background";
const char kBackgroundHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.Timing2.ParseBlockedOnScriptLoad.Background";
const char kBackgroundHistogramParseBlockedOnScriptLoadParseComplete[] =
    "PageLoad.Timing2.ParseBlockedOnScriptLoad.ParseComplete.Background";
const char kBackgroundHistogramParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.Timing2.ParseBlockedOnScriptLoadFromDocumentWrite.Background";
const char
    kBackgroundHistogramParseBlockedOnScriptLoadDocumentWriteParseComplete[] =
        "PageLoad.Timing2.ParseBlockedOnScriptLoadFromDocumentWrite."
        "ParseComplete.Background";

const char kHistogramFirstContentfulPaintHigh[] =
    "PageLoad.Timing2.NavigationToFirstContentfulPaint.HighResolutionClock";
const char kHistogramFirstContentfulPaintLow[] =
    "PageLoad.Timing2.NavigationToFirstContentfulPaint.LowResolutionClock";

const char kHistogramFirstBackground[] =
    "PageLoad.Timing2.NavigationToFirstBackground";
const char kHistogramFirstForeground[] =
    "PageLoad.Timing2.NavigationToFirstForeground";

const char kHistogramBackgroundBeforePaint[] =
    "PageLoad.Timing2.NavigationToFirstBackground.AfterCommit.BeforePaint";
const char kHistogramBackgroundBeforeCommit[] =
    "PageLoad.Timing2.NavigationToFirstBackground.BeforeCommit";
const char kHistogramBackgroundDuringParse[] =
    "PageLoad.Timing2.NavigationToFirstBackground.DuringParse";
const char kHistogramFailedProvisionalLoad[] =
    "PageLoad.Timing2.NavigationToFailedProvisionalLoad";

const char kHistogramForegroundToFirstPaint[] =
    "PageLoad.Timing2.ForegroundToFirstPaint";

const char kRapporMetricsNameCoarseTiming[] =
    "PageLoad.CoarseTiming.NavigationToFirstContentfulPaint";

}  // namespace internal

CorePageLoadMetricsObserver::CorePageLoadMetricsObserver() {}

CorePageLoadMetricsObserver::~CorePageLoadMetricsObserver() {}

void CorePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordTimingHistograms(timing, info);
  RecordRappor(timing, info);
}

void CorePageLoadMetricsObserver::OnFailedProvisionalLoad(
    content::NavigationHandle* navigation_handle) {
  // Only handle actual failures; provisional loads that failed due to another
  // committed load or due to user action are recorded in
  // AbortsPageLoadMetricsObserver.
  net::Error error = navigation_handle->GetNetErrorCode();
  if (error == net::OK || error == net::ERR_ABORTED) {
    return;
  }

  // Saving the related timing and other data in this Observer instead of
  // PageLoadTracker which saves commit and abort times, as it seems
  // not every observer implementation would be interested in this metric.
  failed_provisional_load_info_.interval =
      base::TimeTicks::Now() - navigation_handle->NavigationStart();
  failed_provisional_load_info_.error = error;
}

void CorePageLoadMetricsObserver::RecordTimingHistograms(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // Record metrics for pages which start in the foreground and are
  // backgrounded.
  if (info.started_in_foreground && !info.first_background_time.is_zero()) {
    if (info.time_to_commit.is_zero()) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramBackgroundBeforeCommit,
                          info.first_background_time);
    } else if (timing.first_paint.is_zero() ||
               timing.first_paint > info.first_background_time) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramBackgroundBeforePaint,
                          info.first_background_time);
    }
    if (!timing.parse_start.is_zero() &&
        info.first_background_time >= timing.parse_start &&
        (timing.parse_stop.is_zero() ||
         timing.parse_stop > info.first_background_time)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramBackgroundDuringParse,
                          info.first_background_time);
    }
  }

  if (failed_provisional_load_info_.error != net::OK) {
    // Ignores a background failed provisional load.
    if (WasStartedInForegroundEventInForeground(
            failed_provisional_load_info_.interval, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFailedProvisionalLoad,
                          failed_provisional_load_info_.interval);
    }
  }

  // The rest of the histograms require the load to have committed and be
  // relevant. If |timing.IsEmpty()|, then this load was not tracked by the
  // renderer.
  if (info.time_to_commit.is_zero() || timing.IsEmpty())
    return;

  if (WasStartedInForegroundEventInForeground(info.time_to_commit, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramCommit, info.time_to_commit);
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramCommit,
                        info.time_to_commit);
  }
  if (!timing.dom_content_loaded_event_start.is_zero()) {
    if (WasStartedInForegroundEventInForeground(
            timing.dom_content_loaded_event_start, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramDomContentLoaded,
                          timing.dom_content_loaded_event_start);
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramDomLoadingToDomContentLoaded,
          timing.dom_content_loaded_event_start - timing.dom_loading);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramDomContentLoaded,
                          timing.dom_content_loaded_event_start);
    }
  }
  if (!timing.load_event_start.is_zero()) {
    if (WasStartedInForegroundEventInForeground(timing.load_event_start,
                                                info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramLoad, timing.load_event_start);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramLoad,
                          timing.load_event_start);
    }
  }
  if (!timing.first_layout.is_zero()) {
    if (WasStartedInForegroundEventInForeground(timing.first_layout, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstLayout, timing.first_layout);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstLayout,
                          timing.first_layout);
    }
  }
  if (!timing.first_paint.is_zero()) {
    if (WasStartedInForegroundEventInForeground(timing.first_paint, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstPaint, timing.first_paint);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstPaint,
                          timing.first_paint);
    }

    // Record the time to first paint for pages which were:
    // - Opened in the background.
    // - Moved to the foreground prior to the first paint.
    // - Not moved back to the background prior to the first paint.
    if (!info.started_in_foreground && !info.first_foreground_time.is_zero() &&
        timing.first_paint > info.first_foreground_time &&
        (info.first_background_time.is_zero() ||
         timing.first_paint < info.first_background_time)) {
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramForegroundToFirstPaint,
          timing.first_paint - info.first_foreground_time);
    }
  }
  if (!timing.first_text_paint.is_zero()) {
    if (WasStartedInForegroundEventInForeground(timing.first_text_paint,
                                                info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstTextPaint,
                          timing.first_text_paint);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstTextPaint,
                          timing.first_text_paint);
    }
  }
  if (!timing.first_image_paint.is_zero()) {
    if (WasStartedInForegroundEventInForeground(timing.first_image_paint,
                                                info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstImagePaint,
                          timing.first_image_paint);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstImagePaint,
                          timing.first_image_paint);
    }
  }
  if (!timing.first_contentful_paint.is_zero()) {
    if (WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                                info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaint,
                          timing.first_contentful_paint);
      // Bucket these histograms into high/low resolution clock systems. This
      // might point us to directions that will de-noise some UMA.
      if (base::TimeTicks::IsHighResolution()) {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintHigh,
                            timing.first_contentful_paint);
      } else {
        PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintLow,
                            timing.first_contentful_paint);
      }
      PAGE_LOAD_HISTOGRAM(internal::kHistogramDomLoadingToFirstContentfulPaint,
                          timing.first_contentful_paint - timing.dom_loading);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstContentfulPaint,
                          timing.first_contentful_paint);
    }
  }
  if (!timing.parse_start.is_zero()) {
    if (WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                                info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStartToFirstContentfulPaint,
                          timing.first_contentful_paint - timing.parse_start);
    }

    if (WasParseInForeground(timing.parse_start, timing.parse_stop, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramParseBlockedOnScriptLoad,
                          timing.parse_blocked_on_script_load_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramParseBlockedOnScriptLoadDocumentWrite,
          timing.parse_blocked_on_script_load_from_document_write_duration);
    } else {
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramParseBlockedOnScriptLoad,
          timing.parse_blocked_on_script_load_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramParseBlockedOnScriptLoadDocumentWrite,
          timing.parse_blocked_on_script_load_from_document_write_duration);
    }
  }

  if (!timing.parse_stop.is_zero()) {
    base::TimeDelta parse_duration = timing.parse_stop - timing.parse_start;
    if (WasStartedInForegroundEventInForeground(timing.parse_stop, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramParseDuration, parse_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramParseBlockedOnScriptLoadParseComplete,
          timing.parse_blocked_on_script_load_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::
              kHistogramParseBlockedOnScriptLoadDocumentWriteParseComplete,
          timing.parse_blocked_on_script_load_from_document_write_duration);
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramParseDuration,
                          parse_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramParseBlockedOnScriptLoadParseComplete,
          timing.parse_blocked_on_script_load_duration);
      PAGE_LOAD_HISTOGRAM(
          internal::
              kBackgroundHistogramParseBlockedOnScriptLoadDocumentWriteParseComplete,
          timing.parse_blocked_on_script_load_from_document_write_duration);
    }
  }

  // Log time to first foreground / time to first background. Log counts that we
  // started a relevant page load in the foreground / background.
  if (info.started_in_foreground) {
    if (!info.first_background_time.is_zero())
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstBackground,
                          info.first_background_time);
  } else {
    if (!info.first_foreground_time.is_zero())
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstForeground,
                          info.first_foreground_time);
  }
}

void CorePageLoadMetricsObserver::RecordRappor(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // During the browser process shutdown path, calling
  // BrowserProcess::rappor_service() can reinitialize multiple destroyed
  // objects. This alters shutdown ordering, so we avoid it by testing
  // IsShuttingDown() first.
  if (g_browser_process->IsShuttingDown())
    return;
  rappor::RapporService* rappor_service = g_browser_process->rappor_service();
  if (!rappor_service)
    return;
  if (info.time_to_commit.is_zero())
    return;
  DCHECK(!info.committed_url.is_empty());
  // Log the eTLD+1 of sites that show poor loading performance.
  if (!WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                               info)) {
    return;
  }
  std::unique_ptr<rappor::Sample> sample =
      rappor_service->CreateSample(rappor::UMA_RAPPOR_TYPE);
  sample->SetStringField(
      "Domain", rappor::GetDomainAndRegistrySampleFromGURL(info.committed_url));
  uint64_t bucket_index =
      RapporHistogramBucketIndex(timing.first_contentful_paint);
  sample->SetFlagsField("Bucket", uint64_t(1) << bucket_index,
                        kNumRapporHistogramBuckets);
  // The IsSlow flag is just a one bit boolean if the first contentful paint
  // was > 10s.
  sample->SetFlagsField("IsSlow",
                        timing.first_contentful_paint.InSecondsF() >= 10, 1);
  rappor_service->RecordSampleObj(internal::kRapporMetricsNameCoarseTiming,
                                  std::move(sample));
}
