// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_
#pragma once

#include <string>

#include "base/time.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "googleurl/src/gurl.h"

namespace prerender {

// PrerenderHistograms is responsible for recording all prerender specific
// histograms for PrerenderManager.  It keeps track of the type of prerender
// currently underway (based on the PrerenderOrigin of the most recent
// prerenders, and any experiments detected).
// PrerenderHistograms does not necessarily record all histograms related to
// prerendering, only the ones in the context of PrerenderManager.
class PrerenderHistograms {
 public:
  // Owned by a PrerenderManager object for the lifetime of the
  // PrerenderManager.
  PrerenderHistograms();

  // Records the perceived page load time for a page - effectively the time from
  // when the user navigates to a page to when it finishes loading.  The actual
  // load may have started prior to navigation due to prerender hints.
  void RecordPerceivedPageLoadTime(base::TimeDelta perceived_page_load_time,
                                   bool was_prerender,
                                   bool was_complete_prerender,
                                   const GURL& url);

  // Records the time from when a page starts prerendering to when the user
  // navigates to it. This must be called on the UI thread.
  void RecordTimeUntilUsed(base::TimeDelta time_until_used,
                           base::TimeDelta max_age) const;

  // Record a PerSessionCount data point.
  void RecordPerSessionCount(int count) const;

  // Record time between two prerender requests.
  void RecordTimeBetweenPrerenderRequests(base::TimeDelta time) const;

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin,
                         uint8 experiment_id,
                         FinalStatus final_status) const;


  // To be called when a new prerender is added.
  void RecordPrerender(Origin origin, const GURL& url);

  // Called when we swap in a prerender.
  void RecordUsedPrerender(Origin origin) const;

 private:
  base::TimeTicks GetCurrentTimeTicks() const;

  // Returns the time elapsed since the last prerender happened.
  base::TimeDelta GetTimeSinceLastPrerender() const;

  // Returns whether the PrerenderManager is currently within the prerender
  // window - effectively, up to 30 seconds after a prerender tag has been
  // observed.
  bool WithinWindow() const;

  // Returns the current experiment.
  uint8 GetCurrentExperimentId() const;
  // Returns the current origin.
  Origin GetCurrentOrigin() const;
  // Returns whether or not there is currently an origin/experiment wash.
  bool IsOriginExperimentWash() const;

  // An integer indicating a Prerendering Experiment being currently conducted.
  // (The last experiment ID seen).
  uint8 last_experiment_id_;

  // Origin of the last prerender seen.
  Origin last_origin_;

  // A boolean indicating that we have recently encountered a combination of
  // different experiments and origins, making an attribution of PPLT's to
  // experiments / origins impossible.
  bool origin_experiment_wash_;

  // The time when we last saw a prerender request coming from a renderer.
  // This is used to record perceived PLT's for a certain amount of time
  // from the point that we last saw a <link rel=prerender> tag.
  base::TimeTicks last_prerender_seen_time_;

  // Indicates whether we have recorded page load events after the most
  // recent prerender.  These must be initialized to true, so that we don't
  // start recording events before the first prerender occurs.
  bool seen_any_pageload_;
  bool seen_pageload_started_after_prerender_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderHistograms);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_
