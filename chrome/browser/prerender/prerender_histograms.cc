// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_histograms.h"

#include <string>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "components/google/core/browser/google_util.h"
#include "net/http/http_cache.h"

namespace prerender {

namespace {

// This enum is used to define the buckets for the
// "Prerender.NoStatePrefetchResourceCount" histogram family.
// Hence, existing enumerated constants should never be deleted or reordered,
// and new constants should only be appended at the end of the enumeration.
enum NoStatePrefetchResponseType {
  NO_STORE = 1 << 0,
  REDIRECT = 1 << 1,
  MAIN_RESOURCE = 1 << 2,
  NO_STATE_PREFETCH_RESPONSE_TYPE_COUNT = 1 << 3
};

int GetResourceType(bool is_main_resource, bool is_redirect, bool is_no_store) {
  return (is_no_store * NO_STORE) + (is_redirect * REDIRECT) +
         (is_main_resource * MAIN_RESOURCE);
}

// Time window for which we will record windowed PLTs from the last observed
// link rel=prefetch tag. This is not intended to be the same as the prerender
// ttl, it's just intended to be a window during which a prerender has likely
// affected performance.
const int kWindowDurationSeconds = 30;

std::string ComposeHistogramName(const std::string& prefix_type,
                                 const std::string& name) {
  if (prefix_type.empty())
    return std::string("Prerender.") + name;
  return std::string("Prerender.") + prefix_type + std::string("_") + name;
}

std::string GetHistogramName(Origin origin, const std::string& name) {
  switch (origin) {
    case ORIGIN_OMNIBOX:
      return ComposeHistogramName("omnibox", name);
    case ORIGIN_NONE:
      return ComposeHistogramName("none", name);
    case ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN:
      return ComposeHistogramName("websame", name);
    case ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN:
      return ComposeHistogramName("webcross", name);
    case ORIGIN_EXTERNAL_REQUEST:
      return ComposeHistogramName("externalrequest", name);
    case ORIGIN_INSTANT:
      return ComposeHistogramName("Instant", name);
    case ORIGIN_LINK_REL_NEXT:
      return ComposeHistogramName("webnext", name);
    case ORIGIN_GWS_PRERENDER:
      return ComposeHistogramName("gws", name);
    case ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER:
      return ComposeHistogramName("externalrequestforced", name);
    case ORIGIN_OFFLINE:
      return ComposeHistogramName("offline", name);
    default:
      NOTREACHED();
      break;
  }

  // Dummy return value to make the compiler happy.
  NOTREACHED();
  return ComposeHistogramName("none", name);
}

bool OriginIsOmnibox(Origin origin) {
  return origin == ORIGIN_OMNIBOX;
}

const char* FirstContentfulPaintHiddenName(bool was_hidden) {
  return was_hidden ? ".Hidden" : ".Visible";
}

}  // namespace

// Helper macro for origin-based histogram reporting. All HISTOGRAM arguments
// must be UMA_HISTOGRAM... macros that contain an argument "name" which this
// macro will eventually substitute for the actual name used.
#define PREFIXED_HISTOGRAM(histogram_name, origin, HISTOGRAM)                 \
  do {                                                                        \
    {                                                                         \
      /* Do not rename.  HISTOGRAM expects a local variable "name". */        \
      std::string name = ComposeHistogramName(std::string(), histogram_name); \
      HISTOGRAM;                                                              \
    }                                                                         \
    /* Do not rename.  HISTOGRAM expects a local variable "name". */          \
    std::string name = GetHistogramName(origin, histogram_name);              \
    /* Branching because HISTOGRAM is caching the histogram into a static. */ \
    if (origin == ORIGIN_OMNIBOX) {                                           \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_NONE) {                                       \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN) {              \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN) {             \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_EXTERNAL_REQUEST) {                           \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_INSTANT) {                                    \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_LINK_REL_NEXT) {                              \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER) {          \
      HISTOGRAM;                                                              \
    } else if (origin == ORIGIN_OFFLINE) {                                    \
      HISTOGRAM;                                                              \
    } else {                                                                  \
      HISTOGRAM;                                                              \
    }                                                                         \
  } while (0)

PrerenderHistograms::PrerenderHistograms()
    : seen_any_pageload_(true), seen_pageload_started_after_prerender_(true) {}

void PrerenderHistograms::RecordPrerender() {
  // If we observe multiple tags within the 30 second window, we will still
  // reset the window to begin at the most recent occurrence, so that we will
  // always be in a window in the 30 seconds from each occurrence.
  last_prerender_seen_time_ = GetCurrentTimeTicks();
  seen_any_pageload_ = false;
  seen_pageload_started_after_prerender_ = false;
}

void PrerenderHistograms::RecordPrerenderStarted(Origin origin) const {
  if (OriginIsOmnibox(origin)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Prerender.OmniboxPrerenderCount", 1, 2);
  }
}

void PrerenderHistograms::RecordConcurrency(size_t prerender_count) const {
  static const size_t kMaxRecordableConcurrency = 20;
  DCHECK_GE(kMaxRecordableConcurrency, Config().max_link_concurrency);
  UMA_HISTOGRAM_ENUMERATION(
      base::StringPrintf("Prerender.PrerenderCountOf%" PRIuS "Max",
                         kMaxRecordableConcurrency),
      prerender_count, kMaxRecordableConcurrency + 1);
}

void PrerenderHistograms::RecordUsedPrerender(Origin origin) const {
  if (OriginIsOmnibox(origin)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Prerender.OmniboxNavigationsUsedPrerenderCount", 1, 2);
  }
}

void PrerenderHistograms::RecordTimeSinceLastRecentVisit(
    Origin origin,
    base::TimeDelta delta) const {
  PREFIXED_HISTOGRAM(
      "TimeSinceLastRecentVisit", origin,
      UMA_HISTOGRAM_TIMES(name, delta));
}

base::TimeTicks PrerenderHistograms::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

// Helper macro for histograms.
#define RECORD_PLT(tag, perceived_page_load_time) \
  PREFIXED_HISTOGRAM( \
      tag, origin, \
      UMA_HISTOGRAM_CUSTOM_TIMES( \
        name, \
        perceived_page_load_time, \
        base::TimeDelta::FromMilliseconds(10), \
        base::TimeDelta::FromSeconds(60), \
        100))

// Summary of all histograms Perceived PLT histograms:
// (all prefixed PerceivedPLT)
// PerceivedPLT -- Perceived Pageloadtimes (PPLT) for all pages in the group.
// ...Windowed -- PPLT for pages in the 30s after a prerender is created.
// ...FirstAfterMiss -- First page to finish loading after a prerender, which
// is different from the page that was prerendered.
// ...FirstAfterMissNonOverlapping -- Same as FirstAfterMiss, but only
// triggering for the first page to finish after the prerender that also started
// after the prerender started.
// ...FirstAfterMissBoth -- pages meeting
// FirstAfterMiss AND FirstAfterMissNonOverlapping
// ...FirstAfterMissAnyOnly -- pages meeting
// FirstAfterMiss but NOT FirstAfterMissNonOverlapping
// ..FirstAfterMissNonOverlappingOnly -- pages meeting
// FirstAfterMissNonOverlapping but NOT FirstAfterMiss

void PrerenderHistograms::RecordPerceivedPageLoadTime(
    Origin origin,
    base::TimeDelta perceived_page_load_time,
    NavigationType navigation_type,
    const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  bool within_window = WithinWindow();
  bool is_google_url =
      google_util::IsGoogleDomainUrl(url, google_util::DISALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS);
  RECORD_PLT("PerceivedPLT", perceived_page_load_time);
  if (within_window)
    RECORD_PLT("PerceivedPLTWindowed", perceived_page_load_time);
  if (navigation_type != NAVIGATION_TYPE_NORMAL) {
    DCHECK(navigation_type == NAVIGATION_TYPE_PRERENDERED);
    seen_any_pageload_ = true;
    seen_pageload_started_after_prerender_ = true;
  } else if (within_window) {
    if (!is_google_url) {
      bool recorded_any = false;
      bool recorded_non_overlapping = false;
      if (!seen_any_pageload_) {
        seen_any_pageload_ = true;
        RECORD_PLT("PerceivedPLTFirstAfterMiss", perceived_page_load_time);
        recorded_any = true;
      }
      if (!seen_pageload_started_after_prerender_ &&
          perceived_page_load_time <= GetTimeSinceLastPrerender()) {
        seen_pageload_started_after_prerender_ = true;
        RECORD_PLT("PerceivedPLTFirstAfterMissNonOverlapping",
                   perceived_page_load_time);
        recorded_non_overlapping = true;
      }
      if (recorded_any || recorded_non_overlapping) {
        if (recorded_any && recorded_non_overlapping) {
          RECORD_PLT("PerceivedPLTFirstAfterMissBoth",
                     perceived_page_load_time);
        } else if (recorded_any) {
          RECORD_PLT("PerceivedPLTFirstAfterMissAnyOnly",
                     perceived_page_load_time);
        } else if (recorded_non_overlapping) {
          RECORD_PLT("PerceivedPLTFirstAfterMissNonOverlappingOnly",
                     perceived_page_load_time);
        }
      }
    }
  }
}

void PrerenderHistograms::RecordPerceivedFirstContentfulPaintStatus(
    Origin origin,
    bool successful,
    bool was_hidden) {
  base::UmaHistogramBoolean(GetHistogramName(origin, "PerceivedTTFCPRecorded") +
                                FirstContentfulPaintHiddenName(was_hidden),
                            successful);
}

void PrerenderHistograms::RecordPageLoadTimeNotSwappedIn(
    Origin origin,
    base::TimeDelta page_load_time,
    const GURL& url) const {
  // If the URL to be prerendered is not a http[s] URL, or is a Google URL,
  // do not record.
  if (!url.SchemeIsHTTPOrHTTPS() ||
      google_util::IsGoogleDomainUrl(url, google_util::DISALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    return;
  }
  RECORD_PLT("PrerenderNotSwappedInPLT", page_load_time);
}

void PrerenderHistograms::RecordPercentLoadDoneAtSwapin(Origin origin,
                                                        double fraction) const {
  if (fraction < 0.0 || fraction > 1.0)
    return;
  int percentage = static_cast<int>(fraction * 100);
  if (percentage < 0 || percentage > 100)
    return;
  PREFIXED_HISTOGRAM("PercentLoadDoneAtSwapin",
                     origin, UMA_HISTOGRAM_PERCENTAGE(name, percentage));
}

base::TimeDelta PrerenderHistograms::GetTimeSinceLastPrerender() const {
  return base::TimeTicks::Now() - last_prerender_seen_time_;
}

bool PrerenderHistograms::WithinWindow() const {
  if (last_prerender_seen_time_.is_null())
    return false;
  return GetTimeSinceLastPrerender() <=
      base::TimeDelta::FromSeconds(kWindowDurationSeconds);
}

void PrerenderHistograms::RecordTimeUntilUsed(
    Origin origin,
    base::TimeDelta time_until_used) const {
  PREFIXED_HISTOGRAM(
      "TimeUntilUsed2", origin,
      UMA_HISTOGRAM_CUSTOM_TIMES(
          name,
          time_until_used,
          base::TimeDelta::FromMilliseconds(10),
          base::TimeDelta::FromMinutes(30),
          50));
}

void PrerenderHistograms::RecordAbandonTimeUntilUsed(
    Origin origin,
    base::TimeDelta time_until_used) const {
  PREFIXED_HISTOGRAM(
      "AbandonTimeUntilUsed", origin,
      UMA_HISTOGRAM_CUSTOM_TIMES(
          name,
          time_until_used,
          base::TimeDelta::FromMilliseconds(10),
          base::TimeDelta::FromSeconds(30),
          50));
}

void PrerenderHistograms::RecordPerSessionCount(Origin origin,
                                                int count) const {
  PREFIXED_HISTOGRAM(
      "PrerendersPerSessionCount", origin,
      UMA_HISTOGRAM_COUNTS(name, count));
}

void PrerenderHistograms::RecordTimeBetweenPrerenderRequests(
    Origin origin, base::TimeDelta time) const {
  PREFIXED_HISTOGRAM(
      "TimeBetweenPrerenderRequests", origin,
      UMA_HISTOGRAM_TIMES(name, time));
}

void PrerenderHistograms::RecordFinalStatus(
    Origin origin,
    FinalStatus final_status) const {
  DCHECK(final_status != FINAL_STATUS_MAX);
  PREFIXED_HISTOGRAM(
      "FinalStatus", origin,
      UMA_HISTOGRAM_ENUMERATION(name, final_status, FINAL_STATUS_MAX));
}

void PrerenderHistograms::RecordNetworkBytes(Origin origin,
                                             bool used,
                                             int64_t prerender_bytes,
                                             int64_t profile_bytes) const {
  const int kHistogramMin = 1;
  const int kHistogramMax = 100000000;  // 100M.
  const int kBucketCount = 50;

  UMA_HISTOGRAM_CUSTOM_COUNTS("Prerender.NetworkBytesTotalForProfile",
                              profile_bytes,
                              kHistogramMin,
                              1000000000,  // 1G
                              kBucketCount);

  if (prerender_bytes == 0)
    return;

  if (used) {
    PREFIXED_HISTOGRAM(
        "NetworkBytesUsed",
        origin,
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            name, prerender_bytes, kHistogramMin, kHistogramMax, kBucketCount));
  } else {
    PREFIXED_HISTOGRAM(
        "NetworkBytesWasted",
        origin,
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            name, prerender_bytes, kHistogramMin, kHistogramMax, kBucketCount));
  }
}

void PrerenderHistograms::RecordPrefetchResponseReceived(
    Origin origin,
    bool is_main_resource,
    bool is_redirect,
    bool is_no_store) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  int sample = GetResourceType(is_main_resource, is_redirect, is_no_store);
  std::string histogram_name =
      GetHistogramName(origin, "NoStatePrefetchResponseTypes");
  base::UmaHistogramExactLinear(histogram_name, sample,
                                NO_STATE_PREFETCH_RESPONSE_TYPE_COUNT);
}

void PrerenderHistograms::RecordPrefetchRedirectCount(
    Origin origin,
    bool is_main_resource,
    int redirect_count) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const int kMaxRedirectCount = 10;
  std::string histogram_base_name = base::StringPrintf(
      "NoStatePrefetch%sResourceRedirects", is_main_resource ? "Main" : "Sub");
  std::string histogram_name = GetHistogramName(origin, histogram_base_name);
  base::UmaHistogramExactLinear(histogram_name, redirect_count,
                                kMaxRedirectCount);
}

void PrerenderHistograms::RecordPrefetchFirstContentfulPaintTime(
    Origin origin,
    bool is_no_store,
    bool was_hidden,
    base::TimeDelta time,
    base::TimeDelta prefetch_age) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!prefetch_age.is_zero()) {
    DCHECK_NE(origin, ORIGIN_NONE);
    base::UmaHistogramCustomTimes(GetHistogramName(origin, "PrefetchAge"),
                                  prefetch_age,
                                  base::TimeDelta::FromMilliseconds(10),
                                  base::TimeDelta::FromMinutes(30), 50);
  }

  std::string histogram_base_name;
  if (prefetch_age.is_zero()) {
    histogram_base_name = "PrefetchTTFCP.Reference";
  } else {
    histogram_base_name = prefetch_age < base::TimeDelta::FromMinutes(
                                             net::HttpCache::kPrefetchReuseMins)
                              ? "PrefetchTTFCP.Warm"
                              : "PrefetchTTFCP.Cold";
  }

  histogram_base_name += is_no_store ? ".NoStore" : ".Cacheable";
  histogram_base_name += FirstContentfulPaintHiddenName(was_hidden);
  std::string histogram_name = GetHistogramName(origin, histogram_base_name);

  base::UmaHistogramCustomTimes(histogram_name, time,
                                base::TimeDelta::FromMilliseconds(10),
                                base::TimeDelta::FromMinutes(2), 50);
}

}  // namespace prerender
