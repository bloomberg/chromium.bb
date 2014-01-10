// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "url/gurl.h"

namespace prerender {

class PrerenderManager;
class PrerenderPendingSwapThrottle;
class PrerenderResourceThrottle;
struct RenderViewInfo;

// PrerenderTracker is responsible for keeping track of all prerendering
// RenderViews.
class PrerenderTracker : public base::NonThreadSafe,
                         public PrerenderContents::Observer {
 public:
  typedef std::pair<int, int> ChildRouteIdPair;

  PrerenderTracker();
  virtual ~PrerenderTracker();

  // Returns whether or not a RenderView is prerendering.  Can only be called on
  // the IO thread.  Does not acquire a lock, so may claim a RenderView that has
  // been displayed or destroyed is still prerendering.
  bool IsPrerenderingOnIOThread(int child_id, int route_id) const;

  // Returns whether or not a RenderView and URL are regarding a pending
  // prerender swap. Can only be called on the IO thread. Does not acquire a
  // lock.
  bool IsPendingSwapRequestOnIOThread(int child_id, int route_id,
                                      const GURL& url) const;

  // Called when a PrerenderResourceThrottle defers a request. Cancel
  // or Resume will be called on |throttle| when the prerender is
  // canceled or used, respectively.
  void AddResourceThrottleOnIOThread(
      int child_id, int route_id,
      const base::WeakPtr<PrerenderResourceThrottle>& throttle);

  // Called when a PrerenderResourceThrottle defers a request. Cancel
  // or Resume will be called on |throttle| when the prerender is
  // canceled or used, respectively.
  void AddPendingSwapThrottleOnIOThread(
      int child_id, int route_id, const GURL& url,
      const base::WeakPtr<PrerenderPendingSwapThrottle>& throttle);

  // Called to add throttles for a pending prerender swap.
  void AddPrerenderPendingSwap(const ChildRouteIdPair& child_route_id_pair,
                               const GURL& url);

  // Called to remove the throttles for a pending prerender swap.
  void RemovePrerenderPendingSwap(const ChildRouteIdPair& child_route_id_pair,
                                  bool swap_successful);

 private:
  friend class PrerenderContents;

  // Map of child/route id pairs to final statuses.
  typedef std::map<ChildRouteIdPair, RenderViewInfo> FinalStatusMap;
  // List of throttled requests.
  typedef std::vector<base::WeakPtr<PrerenderResourceThrottle> >
      ResourceThrottleList;
  // Set of throttled requests.
  typedef std::map<ChildRouteIdPair, ResourceThrottleList> ResourceThrottleMap;
  struct PendingSwapThrottleData {
    explicit PendingSwapThrottleData(const GURL& swap_url);
    ~PendingSwapThrottleData();
    GURL url;
    std::vector<base::WeakPtr<PrerenderPendingSwapThrottle> > throttles;
  };
  // Set of throttles for pending swaps.
  typedef std::map<ChildRouteIdPair, PendingSwapThrottleData>
      PendingSwapThrottleMap;

  // From PrerenderContents::Observer:
  virtual void OnPrerenderStart(PrerenderContents* prerender_contents) OVERRIDE;
  virtual void OnPrerenderStop(PrerenderContents* prerender_contents) OVERRIDE;

  // Add/remove the specified pair to |possibly_prerendering_io_thread_set_| on
  // the IO Thread.
  void AddPrerenderOnIOThread(const ChildRouteIdPair& child_route_id_pair);
  void RemovePrerenderOnIOThread(const ChildRouteIdPair& child_route_id_pair,
                                 FinalStatus final_status);

  // Add/remove prerenders pending swap on the IO Thread.
  void AddPrerenderPendingSwapOnIOThread(
      const ChildRouteIdPair& child_route_id_pair, const GURL& url);
  void RemovePrerenderPendingSwapOnIOThread(
      const ChildRouteIdPair& child_route_id_pair,
      bool swap_successful);

  // Tasks posted to the IO Thread to call the above functions.
  static void AddPrerenderOnIOThreadTask(
      const ChildRouteIdPair& child_route_id_pair);
  static void RemovePrerenderOnIOThreadTask(
      const ChildRouteIdPair& child_route_id_pair,
      FinalStatus final_status);
  static void AddPrerenderPendingSwapOnIOThreadTask(
      const ChildRouteIdPair& child_route_id_pair, const GURL& url);
  static void RemovePrerenderPendingSwapOnIOThreadTask(
      const ChildRouteIdPair& child_route_id_pair,
      bool swap_successful);

  static PrerenderTracker* GetDefault();

  // Resources that are throttled, pending a prerender use.  The keys are a
  // superset of child/route id pairs that are prerendering.  Can only access on
  // the IO thread.  May contain entries that have since been displayed.  Used
  // to prevent locking when not needed.
  ResourceThrottleMap resource_throttle_io_thread_map_;

  // Map of pending prerender swaps and their associated throttles,
  // maintained on the IO thread.
  PendingSwapThrottleMap pending_swap_throttle_map_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTracker);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
