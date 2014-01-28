// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tracker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/prerender/prerender_pending_swap_throttle.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace prerender {

PrerenderTracker::PrerenderTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

PrerenderTracker::~PrerenderTracker() {
}

bool PrerenderTracker::IsPendingSwapRequestOnIOThread(
    int render_process_id, int render_frame_id, const GURL& url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ChildRouteIdPair render_frame_route_id_pair(
      render_process_id, render_frame_id);
  PendingSwapThrottleMap::const_iterator it =
      pending_swap_throttle_map_.find(render_frame_route_id_pair);
  return (it != pending_swap_throttle_map_.end() && it->second.url == url);
}

void PrerenderTracker::AddPendingSwapThrottleOnIOThread(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    const base::WeakPtr<PrerenderPendingSwapThrottle>& throttle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ChildRouteIdPair render_frame_route_id_pair(
      render_process_id, render_frame_id);
  PendingSwapThrottleMap::iterator it =
      pending_swap_throttle_map_.find(render_frame_route_id_pair);
  DCHECK(it != pending_swap_throttle_map_.end());
  if (it == pending_swap_throttle_map_.end())
    return;
  CHECK(!it->second.throttle);
  it->second.throttle = throttle;
}

void PrerenderTracker::AddPrerenderPendingSwapOnIOThread(
    const ChildRouteIdPair& render_frame_route_id_pair,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::pair<PendingSwapThrottleMap::iterator, bool> insert_result =
      pending_swap_throttle_map_.insert(std::make_pair(
          render_frame_route_id_pair, PendingSwapThrottleData(url)));
  DCHECK(insert_result.second);
}

void PrerenderTracker::RemovePrerenderPendingSwapOnIOThread(
    const ChildRouteIdPair& render_frame_route_id_pair,
    bool swap_successful) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  PendingSwapThrottleMap::iterator it =
      pending_swap_throttle_map_.find(render_frame_route_id_pair);
  DCHECK(it != pending_swap_throttle_map_.end());
  // Cancel or resume all throttled resources.
  if (it->second.throttle) {
    if (swap_successful)
      it->second.throttle->Cancel();
    else
      it->second.throttle->Resume();
  }
  pending_swap_throttle_map_.erase(render_frame_route_id_pair);
}

void PrerenderTracker::AddPrerenderPendingSwap(
    const ChildRouteIdPair& render_frame_route_id_pair,
    const GURL& url) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrerenderTracker::AddPrerenderPendingSwapOnIOThread,
                 base::Unretained(this), render_frame_route_id_pair, url));
}

void PrerenderTracker::RemovePrerenderPendingSwap(
    const ChildRouteIdPair& render_frame_route_id_pair,
    bool swap_successful) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrerenderTracker::RemovePrerenderPendingSwapOnIOThread,
                 base::Unretained(this), render_frame_route_id_pair,
                 swap_successful));
}

PrerenderTracker::PendingSwapThrottleData::PendingSwapThrottleData(
    const GURL& swap_url)
    : url(swap_url) {
}

PrerenderTracker::PendingSwapThrottleData::~PendingSwapThrottleData() {
}

}  // namespace prerender
