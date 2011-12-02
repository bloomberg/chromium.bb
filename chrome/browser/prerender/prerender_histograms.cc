// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_histograms.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/prerender/prerender_field_trial.h"

namespace prerender {

namespace {

// Time window for which we will record windowed PLT's from the last
// observed link rel=prefetch tag.
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
    case ORIGIN_OMNIBOX_ORIGINAL:
      return ComposeHistogramName("omnibox_original", name);
    case ORIGIN_OMNIBOX_CONSERVATIVE:
      return ComposeHistogramName("omnibox_conservative", name);
    case ORIGIN_OMNIBOX_EXACT:
      return ComposeHistogramName("omnibox_exact", name);
    case ORIGIN_OMNIBOX_EXACT_FULL:
      return ComposeHistogramName("omnibox_exact_full", name);
    case ORIGIN_LINK_REL_PRERENDER:
      return ComposeHistogramName("web", name);
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
  return origin == ORIGIN_OMNIBOX_ORIGINAL ||
         origin == ORIGIN_OMNIBOX_CONSERVATIVE ||
         origin == ORIGIN_OMNIBOX_EXACT ||
         origin == ORIGIN_OMNIBOX_EXACT_FULL;
}

}  // namespace

// Helper macros for experiment-based and origin-based histogram reporting.
// All HISTOGRAM arguments must be UMA_HISTOGRAM... macros that contain an
// argument "name" which these macros will eventually substitute for the
// actual name used.
#define PREFIXED_HISTOGRAM(histogram_name, HISTOGRAM) \
  PREFIXED_HISTOGRAM_INTERNAL(GetCurrentOrigin(), GetCurrentExperimentId(), \
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
  } else if (origin == ORIGIN_LINK_REL_PRERENDER) { \
    HISTOGRAM; \
  } else if (origin == ORIGIN_OMNIBOX_ORIGINAL || \
             origin == ORIGIN_OMNIBOX_CONSERVATIVE || \
             origin == ORIGIN_OMNIBOX_EXACT || \
             origin == ORIGIN_OMNIBOX_EXACT_FULL) { \
    HISTOGRAM; \
  } else if (experiment != kNoExperiment) { \
    HISTOGRAM; \
  } else { \
    HISTOGRAM; \
  } \
}

PrerenderHistograms::PrerenderHistograms()
    : last_experiment_id_(kNoExperiment),
      last_origin_(ORIGIN_LINK_REL_PRERENDER),
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
    UMA_HISTOGRAM_COUNTS("Prerender.OmniboxPrerenderCount_" +
                         GetOmniboxHistogramSuffix(), 1);
  }
}

void PrerenderHistograms::RecordUsedPrerender(Origin origin) const {
  if (OriginIsOmnibox(origin)) {
    UMA_HISTOGRAM_COUNTS("Prerender.OmniboxNavigationsUsedPrerenderCount_" +
                         GetOmniboxHistogramSuffix(), 1);
  }
}

base::TimeTicks PrerenderHistograms::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

// Helper macro for histograms.
#define RECORD_PLT(tag, perceived_page_load_time) { \
  PREFIXED_HISTOGRAM( \
    base::FieldTrial::MakeName(tag, "Prefetch"), \
    UMA_HISTOGRAM_CUSTOM_TIMES( \
        name, \
        perceived_page_load_time, \
        base::TimeDelta::FromMilliseconds(10), \
        base::TimeDelta::FromSeconds(60), \
        100)); \
}

void PrerenderHistograms::RecordPerceivedPageLoadTime(
    base::TimeDelta perceived_page_load_time, bool was_prerender,
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
    base::TimeDelta time_until_used, base::TimeDelta max_age) const {
  PREFIXED_HISTOGRAM(
      "TimeUntilUsed",
      UMA_HISTOGRAM_CUSTOM_TIMES(
          name,
          time_until_used,
          base::TimeDelta::FromMilliseconds(10),
          max_age,
          50));
}

void PrerenderHistograms::RecordPerSessionCount(int count) const {
  PREFIXED_HISTOGRAM(
      "PrerendersPerSessionCount",
      UMA_HISTOGRAM_COUNTS(name, count));
}

void PrerenderHistograms::RecordTimeBetweenPrerenderRequests(
    base::TimeDelta time) const {
  PREFIXED_HISTOGRAM(
      "TimeBetweenPrerenderRequests",
      UMA_HISTOGRAM_TIMES(name, time));
}

void PrerenderHistograms::RecordFinalStatus(Origin origin,
                                            uint8 experiment_id,
                                            FinalStatus final_status) const {
  DCHECK(final_status != FINAL_STATUS_MAX);
  PREFIXED_HISTOGRAM_ORIGIN_EXPERIMENT(
      "FinalStatus", origin, experiment_id,
      UMA_HISTOGRAM_ENUMERATION(name, final_status, FINAL_STATUS_MAX));
}

uint8 PrerenderHistograms::GetCurrentExperimentId() const {
  if (!WithinWindow())
    return kNoExperiment;
  return last_experiment_id_;
}

Origin PrerenderHistograms::GetCurrentOrigin() const {
  if (!WithinWindow())
    return ORIGIN_LINK_REL_PRERENDER;
  return last_origin_;
}

bool PrerenderHistograms::IsOriginExperimentWash() const {
  if (!WithinWindow())
    return false;
  return origin_experiment_wash_;
}

}  // namespace prerender
