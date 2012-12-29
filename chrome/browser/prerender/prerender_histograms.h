// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_

#include <string>

#include "base/time.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_local_predictor.h"
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
  void RecordPerceivedPageLoadTime(Origin origin,
                                   base::TimeDelta perceived_page_load_time,
                                   bool was_prerender,
                                   bool was_complete_prerender,
                                   const GURL& url);

  // Records, in a histogram, the percentage of the page load time that had
  // elapsed by the time it is swapped in.  Values outside of [0, 1.0] are
  // invalid and ignored.
  void RecordPercentLoadDoneAtSwapin(Origin origin, double fraction) const;

  // Records the actual pageload time of a prerender that has not been swapped
  // in yet, but finished loading.
  void RecordPageLoadTimeNotSwappedIn(Origin origin,
                                      base::TimeDelta page_load_time,
                                      const GURL& url) const;

  // Records the time from when a page starts prerendering to when the user
  // navigates to it. This must be called on the UI thread.
  void RecordTimeUntilUsed(Origin origin,
                           base::TimeDelta time_until_used) const;

  // Record a PerSessionCount data point.
  void RecordPerSessionCount(Origin origin, int count) const;

  // Record time between two prerender requests.
  void RecordTimeBetweenPrerenderRequests(Origin origin,
                                          base::TimeDelta time) const;

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin,
                         uint8 experiment_id,
                         PrerenderContents::MatchCompleteStatus mc_status,
                         FinalStatus final_status) const;

  // To be called when a new prerender is added.
  void RecordPrerender(Origin origin, const GURL& url);

  // To be called when a new prerender is started.
  void RecordPrerenderStarted(Origin origin) const;

  // To be called when we know how many prerenders are running after starting
  // a prerender.
  void RecordConcurrency(size_t prerender_count) const;

  // Called when we swap in a prerender.
  void RecordUsedPrerender(Origin origin) const;

  // Record the time since a page was recently visited.
  void RecordTimeSinceLastRecentVisit(Origin origin,
                                      base::TimeDelta time) const;

  // Record a percentage of pixels of the final page already in place at
  // swap-in.
  void RecordFractionPixelsFinalAtSwapin(Origin origin, double fraction) const;

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
