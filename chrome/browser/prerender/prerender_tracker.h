// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_

#include <map>
#include <set>
#include <utility>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/prerender/prerender_cookie_store.h"
#include "content/public/browser/render_process_host_observer.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace prerender {

class PrerenderPendingSwapThrottle;

// Global object for maintaining various prerender state on the IO thread.
class PrerenderTracker {
 public:
  typedef std::pair<int, int> ChildRouteIdPair;

  PrerenderTracker();
  virtual ~PrerenderTracker();

  // Returns whether or not a RenderFrame and URL are regarding a pending
  // prerender swap. Can only be called on the IO thread.
  bool IsPendingSwapRequestOnIOThread(int render_process_id,
                                      int render_frame_id,
                                      const GURL& url) const;

  // Called when a PrerenderPendingSwapThrottle defers a request. Cancel or
  // Resume will be called on |throttle| when the prerender is canceled or used,
  // respectively.
  void AddPendingSwapThrottleOnIOThread(
      int render_process_id, int render_frame_id, const GURL& url,
      const base::WeakPtr<PrerenderPendingSwapThrottle>& throttle);

  // Called to add throttles for a pending prerender swap.
  void AddPrerenderPendingSwap(
      const ChildRouteIdPair& render_frame_route_id_pair,
      const GURL& url);

  // Called to remove the throttles for a pending prerender swap.
  void RemovePrerenderPendingSwap(
      const ChildRouteIdPair& render_frame_route_id_pair,
      bool swap_successful);

  // Gets the Prerender Cookie Store for a specific render process, if it
  // is a prerender. Only to be called from the IO thread.
  scoped_refptr<PrerenderCookieStore> GetPrerenderCookieStoreForRenderProcess(
      int process_id);

  // Called when a given render process has changed a cookie for |url|,
  // in |cookie_monster|.
  // Only to be called from the IO thread.
  void OnCookieChangedForURL(int process_id,
                             net::CookieMonster* cookie_monster,
                             const GURL& url);

  void AddPrerenderCookieStoreOnIOThread(
      int process_id,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      const base::Closure& cookie_conflict_cb);

  void RemovePrerenderCookieStoreOnIOThread(int process_id, bool was_swapped);

 private:
  // Add/remove prerenders pending swap on the IO Thread.
  void AddPrerenderPendingSwapOnIOThread(
      const ChildRouteIdPair& render_frame_route_id_pair, const GURL& url);
  void RemovePrerenderPendingSwapOnIOThread(
      const ChildRouteIdPair& render_frame_route_id_pair,
      bool swap_successful);

  struct PendingSwapThrottleData {
    explicit PendingSwapThrottleData(const GURL& swap_url);
    ~PendingSwapThrottleData();
    GURL url;
    base::WeakPtr<PrerenderPendingSwapThrottle> throttle;
  };

  // Map of pending prerender swaps and their associated throttles,
  // maintained on the IO thread. The key is the routing ID pair
  // of a RenderFrame.
  typedef std::map<ChildRouteIdPair, PendingSwapThrottleData>
      PendingSwapThrottleMap;
  PendingSwapThrottleMap pending_swap_throttle_map_;

  // Map of prerendering render process ids to PrerenderCookieStore used for
  // the prerender. Only to be used on the IO thread.
  typedef base::hash_map<int, scoped_refptr<PrerenderCookieStore> >
      PrerenderCookieStoreMap;
  PrerenderCookieStoreMap prerender_cookie_store_map_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTracker);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
