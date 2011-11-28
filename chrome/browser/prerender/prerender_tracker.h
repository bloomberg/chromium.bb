// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
#pragma once

#include <map>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "googleurl/src/gurl.h"

namespace prerender {

class PrerenderManager;
struct RenderViewInfo;

// An URLCounter keeps track of the number of occurrences of a prerendered URL.
class URLCounter : public base::NonThreadSafe {
 public:
  URLCounter();
  ~URLCounter();

  // Determines whether the URL is contained in the set.
  bool MatchesURL(const GURL& url) const;

  // Adds a URL to the set.
  void AddURL(const GURL& url);

  // Removes a number of URLs from the set.
  void RemoveURLs(const std::vector<GURL>& urls);

 private:
  typedef std::map<GURL, int> URLCountMap;
  URLCountMap url_count_map_;
};

// PrerenderTracker is responsible for keeping track of all prerendering
// RenderViews and their statuses.  Its list is guaranteed to be up to date
// and can be modified on any thread.
class PrerenderTracker {
 public:
  PrerenderTracker();
  ~PrerenderTracker();

  // Attempts to set the status of the specified RenderViewHost to
  // FINAL_STATUS_USED.  Returns true on success.  Returns false if it has
  // already been cancelled for any reason or is no longer prerendering.
  // Can only be called only on the IO thread.  This method will not call
  // PrerenderContents::set_final_status() on the corresponding
  // PrerenderContents.
  //
  // If it returns true, all subsequent calls to TryCancel and TryUse for the
  // RenderView will return false.
  bool TryUse(int child_id, int route_id);

  // Attempts to cancel prerendering by the specified RenderView, setting the
  // FinalStatus to |final_status|.  Returns true if the specified prerender has
  // been cancelled, either as a result of this call or for any other reason.
  // If the call results in cancelling a PrerenderContents, a task to destroy
  // it is also posted to the UI thread.
  //
  // When true is returned, it is guaranteed that the RenderView will never
  // be displayed.  When false is returned, the RenderView has either been
  // swapped into a tab or has already been destroyed.
  bool TryCancel(int child_id, int route_id, FinalStatus final_status);

  // Same as above, but can only called on the IO Thread.  Does not acquire a
  // lock when the RenderView is not being prerendered.
  bool TryCancelOnIOThread(int child_id, int route_id,
                           FinalStatus final_status);

  // Potentially delay a resource request on the IO thread to prevent a double
  // get.
  bool PotentiallyDelayRequestOnIOThread(
      const GURL& gurl,
      int child_id,
      int route_id,
      int request_id);

  void AddPrerenderURLOnUIThread(const GURL& url);
  void RemovePrerenderURLsOnUIThread(const std::vector<GURL>& urls);

  // Gets the FinalStatus of the specified prerendered RenderView.  Returns
  // |true| and sets |final_status| to the status of the RenderView if it
  // is found, returns false otherwise.
  bool GetFinalStatus(int child_id, int route_id,
                      FinalStatus* final_status) const;

  // Returns whether or not a RenderView is prerendering.  Can only be called on
  // the IO thread.  Does not acquire a lock, so may claim a RenderView that has
  // been displayed or destroyed is still prerendering.
  bool IsPrerenderingOnIOThread(int child_id, int route_id) const;

 private:
  friend class PrerenderContents;
  FRIEND_TEST_ALL_PREFIXES(PrerenderTrackerTest, PrerenderTrackerNull);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTrackerTest, PrerenderTrackerUsed);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTrackerTest, PrerenderTrackerCancelled);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTrackerTest, PrerenderTrackerCancelledOnIO);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTrackerTest, PrerenderTrackerCancelledFast);
  FRIEND_TEST_ALL_PREFIXES(PrerenderTrackerTest, PrerenderTrackerMultiple);

  typedef std::pair<int, int> ChildRouteIdPair;
  // Map of child/route id pairs to final statuses.
  typedef std::map<ChildRouteIdPair, RenderViewInfo> FinalStatusMap;
  // Set of child/route id pairs that may be prerendering.
  typedef std::set<ChildRouteIdPair> PossiblyPrerenderingChildRouteIdPairs;

  // Must be called when a RenderView starts prerendering, before the first
  // navigation starts to avoid any races.
  void OnPrerenderingStarted(int child_id, int route_id,
                             PrerenderManager* prerender_manager);

  // Must be called when a RenderView stops prerendering, either because the
  // RenderView was used or prerendering was cancelled and it is being
  // destroyed.
  void OnPrerenderingFinished(int child_id, int route_id);

  // Attempts to set the FinalStatus of the specified RenderView to
  // |desired_final_status|.  If non-NULL, |actual_final_status| is set to the
  // FinalStatus of the RenderView.
  //
  // If the FinalStatus of the RenderView is successfully set, returns true and
  // sets |actual_final_status| to |desired_final_status|.
  //
  // If the FinalStatus of the RenderView was already set, returns false and
  // sets |actual_final_status| to the actual FinalStatus of the RenderView.
  //
  // If the RenderView is not a prerendering RenderView, returns false and sets
  // |actual_final_status| to FINAL_STATUS_MAX.
  bool SetFinalStatus(int child_id, int route_id,
                      FinalStatus desired_final_status,
                      FinalStatus* actual_final_status);

  // Add/remove the specified pair to |possibly_prerendering_io_thread_set_| on
  // the IO Thread.
  void AddPrerenderOnIOThread(const ChildRouteIdPair& child_route_id_pair);
  void RemovePrerenderOnIOThread(const ChildRouteIdPair& child_route_id_pair);

  // Tasks posted to the IO Thread to call the above functions.
  static void AddPrerenderOnIOThreadTask(
      const ChildRouteIdPair& child_route_id_pair);
  static void RemovePrerenderOnIOThreadTask(
      const ChildRouteIdPair& child_route_id_pair);

  static PrerenderTracker* GetDefault();

  // |final_status_map_lock_| protects access to |final_status_map_|.
  mutable base::Lock final_status_map_lock_;
  // Map containing child/route id pairs and their final statuses.  Must only be
  // accessed while the lock is held.  Values are always accurate and up to
  // date.
  FinalStatusMap final_status_map_;

  // Superset of child/route id pairs that are prerendering.  Can only access on
  // the IO thread.  May contain entries that have since been displayed.  Only
  // used to prevent locking when not needed.
  PossiblyPrerenderingChildRouteIdPairs possibly_prerendering_io_thread_set_;

  // |url_counter_| keeps track of the top-level URLs which are being
  // prerendered. It must only be accessed on the IO thread.
  URLCounter url_counter_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTracker);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
