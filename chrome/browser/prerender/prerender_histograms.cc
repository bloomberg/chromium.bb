// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_histograms.h"

#include <string>

#include "base/format_macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_util.h"

using predictors::AutocompleteActionPredictor;

namespace prerender {

namespace {

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

std::string GetHistogramName(Origin origin, uint8 experiment_id,
                             bool is_wash, const std::string& name) {
  if (is_wash)
    return ComposeHistogramName("wash", name);

  if (origin == ORIGIN_GWS_PRERENDER) {
    if (experiment_id == kNoExperiment)
      return ComposeHistogramName("gws", name);
    return ComposeHistogramName("exp" + std::string(1, experiment_id + '0'),
                                name);
  }

  if (experiment_id != kNoExperiment)
    return ComposeHistogramName("wash", name);

  switch (origin) {
    case ORIGIN_OMNIBOX:
      return ComposeHistogramName("omnibox", name);
    case ORIGIN_NONE:
      return ComposeHistogramName("none", name);
    case ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN:
      return ComposeHistogramName("websame", name);
    case ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN:
      return ComposeHistogramName("webcross", name);
    case ORIGIN_GWS_PRERENDER:  // Handled above.
    default:
      NOTREACHED();
      break;
  };

  // Dummy return value to make the compiler happy.
  NOTREACHED();
  return ComposeHistogramName("wash", name);
}

bool OriginIsOmnibox(Origin origin) {
  return origin == ORIGIN_OMNIBOX;
}

}  // namespace

// Helper macros for experiment-based and origin-based histogram reporting.
// All HISTOGRAM arguments must be UMA_HISTOGRAM... macros that contain an
// argument "name" which these macros will eventually substitute for the
// actual name used.
#define PREFIXED_HISTOGRAM(histogram_name, origin, HISTOGRAM)           \
  PREFIXED_HISTOGRAM_INTERNAL(origin, GetCurrentExperimentId(),         \
                              IsOriginExperimentWash(), HISTOGRAM, \
                              histogram_name)

#define PREFIXED_HISTOGRAM_ORIGIN_EXPERIMENT(histogram_name, origin, \
                                             experiment, HISTOGRAM) \
  PREFIXED_HISTOGRAM_INTERNAL(origin, experiment, false, HISTOGRAM, \
                              histogram_name)

#define PREFIXED_HISTOGRAM_INTERNAL(origin, experiment, wash, HISTOGRAM, \
                                    histogram_name) { \
  { \
    /* Do not rename.  HISTOGRAM expects a local variable "name". */ \
    std::string name = ComposeHistogramName("", histogram_name); \
    HISTOGRAM; \
  } \
  /* Do not rename.  HISTOGRAM expects a local variable "name". */ \
  std::string name = GetHistogramName(origin, experiment, wash, \
                                      histogram_name); \
  static uint8 recording_experiment = kNoExperiment; \
  if (recording_experiment == kNoExperiment && experiment != kNoExperiment) \
    recording_experiment = experiment; \
  if (wash) { \
    HISTOGRAM; \
  } else if (experiment != kNoExperiment && \
             (origin != ORIGIN_GWS_PRERENDER || \
              experiment != recording_experiment)) { \
  } else if (origin == ORIGIN_OMNIBOX) { \
    HISTOGRAM; \
  } else if (origin == ORIGIN_NONE) { \
    HISTOGRAM; \
  } else if (origin == ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN) { \
    HISTOGRAM; \
  } else if (origin == ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN) { \
    HISTOGRAM; \
  } else if (experiment != kNoExperiment) { \
    HISTOGRAM; \
  } else { \
    HISTOGRAM; \
  } \
}

PrerenderHistograms::PrerenderHistograms()
    : last_experiment_id_(kNoExperiment),
      last_origin_(ORIGIN_MAX),
      origin_experiment_wash_(false),
      seen_any_pageload_(true),
      seen_pageload_started_after_prerender_(true) {
}

void PrerenderHistograms::RecordPrerender(Origin origin, const GURL& url) {
  // Check if we are doing an experiment.
  uint8 experiment = GetQueryStringBasedExperiment(url);

  // We need to update last_experiment_id_, last_origin_, and
  // origin_experiment_wash_.
  if (!WithinWindow()) {
    // If we are outside a window, this is a fresh start and we are fine,
    // and there is no mix.
    origin_experiment_wash_ = false;
  } else {
    // If we are inside the last window, there is a mish mash of origins
    // and experiments if either there was a mish mash before, or the current
    // experiment/origin does not match the previous one.
    if (experiment != last_experiment_id_ || origin != last_origin_)
      origin_experiment_wash_ = true;
  }

  last_origin_ = origin;
  last_experiment_id_ = experiment;

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
        StringPrintf("Prerender.OmniboxPrerenderCount%s",
                     PrerenderManager::GetModeString()), 1, 2);
  }
}

void PrerenderHistograms::RecordConcurrency(size_t prerender_count) const {
  static const size_t kMaxRecordableConcurrency = 20;
  DCHECK_GE(kMaxRecordableConcurrency, Config().max_link_concurrency);
  UMA_HISTOGRAM_ENUMERATION(
      StringPrintf("Prerender.PrerenderCountOf%" PRIuS "Max",
                   kMaxRecordableConcurrency),
      prerender_count, kMaxRecordableConcurrency + 1);
}

void PrerenderHistograms::RecordUsedPrerender(Origin origin) const {
  if (OriginIsOmnibox(origin)) {
    UMA_HISTOGRAM_ENUMERATION(
        StringPrintf("Prerender.OmniboxNavigationsUsedPrerenderCount%s",
                     PrerenderManager::GetModeString()), 1, 2);
  }
}

void PrerenderHistograms::RecordTimeSinceLastRecentVisit(
    Origin origin,
    base::TimeDelta delta) const {
  PREFIXED_HISTOGRAM(
      "TimeSinceLastRecentVisit", origin,
      UMA_HISTOGRAM_TIMES(name, delta));
}

void PrerenderHistograms::RecordFractionPixelsFinalAtSwapin(
    Origin origin,
    double fraction) const {
  if (fraction < 0.0 || fraction > 1.0)
    return;
  int percentage = static_cast<int>(fraction * 100);
  if (percentage < 0 || percentage > 100)
    return;
  PREFIXED_HISTOGRAM(
      base::FieldTrial::MakeName("FractionPixelsFinalAtSwapin", "Prerender"),
      origin, UMA_HISTOGRAM_PERCENTAGE(name, percentage));
}

base::TimeTicks PrerenderHistograms::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

// Helper macro for histograms.
#define RECORD_PLT(tag, perceived_page_load_time) { \
  PREFIXED_HISTOGRAM( \
      base::FieldTrial::MakeName(tag, "Prerender"), origin, \
      UMA_HISTOGRAM_CUSTOM_TIMES( \
        name, \
        perceived_page_load_time, \
        base::TimeDelta::FromMilliseconds(10), \
        base::TimeDelta::FromSeconds(60), \
        100)); \
}

// Summary of all histograms Perceived PLT histograms:
// (all prefixed PerceivedPLT)
// PerceivedPLT -- Perceived Pageloadtimes (PPLT) for all pages in the group.
// ...Windowed -- PPLT for pages in the 30s after a prerender is created.
// ...Matched -- A prerendered page that was swapped in.  In the NoUse
// and Control group cases, while nothing ever gets swapped in, we do keep
// track of what would be prerendered and would be swapped in -- and those
// cases are what is classified as Match for these groups.
// ...MatchedComplete -- A prerendered page that was swapped in + a few
// that were not swapped in so that the set of pages lines up more closely with
// the control group.
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
    bool was_prerender,
    bool was_complete_prerender, const GURL& url) {
  if (!IsWebURL(url))
    return;
  bool within_window = WithinWindow();
  bool is_google_url = IsGoogleDomain(url);
  RECORD_PLT("PerceivedPLT", perceived_page_load_time);
  if (within_window)
    RECORD_PLT("PerceivedPLTWindowed", perceived_page_load_time);
  if (was_prerender || was_complete_prerender) {
    if (was_prerender)
      RECORD_PLT("PerceivedPLTMatched", perceived_page_load_time);
    if (was_complete_prerender)
      RECORD_PLT("PerceivedPLTMatchedComplete", perceived_page_load_time);
    seen_any_pageload_ = true;
    seen_pageload_started_after_prerender_ = true;
  } else if (within_window) {
    RECORD_PLT("PerceivedPLTWindowNotMatched", perceived_page_load_time);
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

void PrerenderHistograms::RecordPageLoadTimeNotSwappedIn(
    Origin origin,
    base::TimeDelta page_load_time,
    const GURL& url) const {
  // If the URL to be prerendered is not a http[s] URL, or is a Google URL,
  // do not record.
  if (!IsWebURL(url) || IsGoogleDomain(url))
    return;
  RECORD_PLT("PrerenderNotSwappedInPLT", page_load_time);
}

void PrerenderHistograms::RecordPercentLoadDoneAtSwapin(Origin origin,
                                                        double fraction) const {
  if (fraction < 0.0 || fraction > 1.0)
    return;
  int percentage = static_cast<int>(fraction * 100);
  if (percentage < 0 || percentage > 100)
    return;
  PREFIXED_HISTOGRAM(
      base::FieldTrial::MakeName("PercentLoadDoneAtSwapin", "Prerender"),
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
    uint8 experiment_id,
    PrerenderContents::MatchCompleteStatus mc_status,
    FinalStatus final_status) const {
  DCHECK(final_status != FINAL_STATUS_MAX);

  if (mc_status == PrerenderContents::MATCH_COMPLETE_DEFAULT ||
      mc_status == PrerenderContents::MATCH_COMPLETE_REPLACED) {
    PREFIXED_HISTOGRAM_ORIGIN_EXPERIMENT(
        base::FieldTrial::MakeName("FinalStatus", "Prerender"),
        origin, experiment_id,
        UMA_HISTOGRAM_ENUMERATION(name, final_status, FINAL_STATUS_MAX));
  }
  if (mc_status == PrerenderContents::MATCH_COMPLETE_DEFAULT ||
      mc_status == PrerenderContents::MATCH_COMPLETE_REPLACEMENT ||
      mc_status == PrerenderContents::MATCH_COMPLETE_REPLACEMENT_PENDING) {
    PREFIXED_HISTOGRAM_ORIGIN_EXPERIMENT(
        base::FieldTrial::MakeName("FinalStatusMatchComplete", "Prerender"),
        origin, experiment_id,
        UMA_HISTOGRAM_ENUMERATION(name, final_status, FINAL_STATUS_MAX));
  }
}

uint8 PrerenderHistograms::GetCurrentExperimentId() const {
  if (!WithinWindow())
    return kNoExperiment;
  return last_experiment_id_;
}

bool PrerenderHistograms::IsOriginExperimentWash() const {
  if (!WithinWindow())
    return false;
  return origin_experiment_wash_;
}

}  // namespace prerender
