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
    default:
      NOTREACHED();
      break;
  }

  // Dummy return value to make the compiler happy.
  NOTREACHED();
  return ComposeHistogramName("none", name);
}

const char* FirstContentfulPaintHiddenName(bool was_hidden) {
  return was_hidden ? ".Hidden" : ".Visible";
}

}  // namespace

PrerenderHistograms::PrerenderHistograms() {}

void PrerenderHistograms::RecordPerceivedFirstContentfulPaintStatus(
    Origin origin,
    bool successful,
    bool was_hidden) const {
  base::UmaHistogramBoolean(GetHistogramName(origin, "PerceivedTTFCPRecorded") +
                                FirstContentfulPaintHiddenName(was_hidden),
                            successful);
}

void PrerenderHistograms::RecordFinalStatus(
    Origin origin,
    FinalStatus final_status) const {
  DCHECK(final_status != FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(GetHistogramName(origin, "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
  base::UmaHistogramEnumeration(ComposeHistogramName("", "FinalStatus"),
                                final_status, FINAL_STATUS_MAX);
}

void PrerenderHistograms::RecordNetworkBytesConsumed(
    Origin origin,
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

  base::UmaHistogramCustomCounts(GetHistogramName(origin, "NetworkBytesWasted"),
                                 prerender_bytes, kHistogramMin, kHistogramMax,
                                 kBucketCount);
  base::UmaHistogramCustomCounts(ComposeHistogramName("", "NetworkBytesWasted"),
                                 prerender_bytes, kHistogramMin, kHistogramMax,
                                 kBucketCount);
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
    base::TimeDelta prefetch_age) const {
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
