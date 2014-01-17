// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tracker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_pending_swap_throttle.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "net/base/load_flags.h"

using content::BrowserThread;

namespace prerender {

struct RenderViewInfo {
  explicit RenderViewInfo(PrerenderManager* prerender_manager)
      : final_status(FINAL_STATUS_MAX),
        prerender_manager(prerender_manager->AsWeakPtr()) {
  }
  ~RenderViewInfo() {}

  FinalStatus final_status;
  base::WeakPtr<PrerenderManager> prerender_manager;
};

PrerenderTracker::PrerenderTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

PrerenderTracker::~PrerenderTracker() {
}

void PrerenderTracker::OnPrerenderStart(
    PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  int child_id, route_id;
  bool got_child_id = prerender_contents->GetChildId(&child_id);
  DCHECK(got_child_id);
  bool got_route_id = prerender_contents->GetRouteId(&route_id);
  DCHECK(got_route_id);

  ChildRouteIdPair child_route_id_pair(child_id, route_id);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrerenderTracker::AddPrerenderOnIOThread,
                 base::Unretained(this), child_route_id_pair));
}

void PrerenderTracker::OnPrerenderStop(
    PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  int child_id, route_id;
  bool got_child_id = prerender_contents->GetChildId(&child_id);
  DCHECK(got_child_id);
  bool got_route_id = prerender_contents->GetRouteId(&route_id);
  DCHECK(got_route_id);

  ChildRouteIdPair child_route_id_pair(child_id, route_id);

  DCHECK_LT(prerender_contents->final_status(), FINAL_STATUS_MAX);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrerenderTracker::RemovePrerenderOnIOThread,
                 base::Unretained(this), child_route_id_pair,
                 prerender_contents->final_status()));
}

bool PrerenderTracker::IsPrerenderingOnIOThread(int child_id,
                                                int route_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ChildRouteIdPair child_route_id_pair(child_id, route_id);
  return resource_throttle_io_thread_map_.count(child_route_id_pair) > 0;
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

void PrerenderTracker::AddResourceThrottleOnIOThread(
    int child_id,
    int route_id,
    const base::WeakPtr<PrerenderResourceThrottle>& throttle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ChildRouteIdPair child_route_id_pair(child_id, route_id);
  ResourceThrottleMap::iterator resource_throttle_map_it =
      resource_throttle_io_thread_map_.find(child_route_id_pair);
  DCHECK(resource_throttle_map_it != resource_throttle_io_thread_map_.end());
  resource_throttle_map_it->second.push_back(throttle);
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

void PrerenderTracker::AddPrerenderOnIOThread(
    const ChildRouteIdPair& child_route_id_pair) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!IsPrerenderingOnIOThread(child_route_id_pair.first,
                                   child_route_id_pair.second));

  resource_throttle_io_thread_map_.insert(
      std::make_pair(child_route_id_pair, ResourceThrottleList()));
}

void PrerenderTracker::RemovePrerenderOnIOThread(
    const ChildRouteIdPair& child_route_id_pair,
    FinalStatus final_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(IsPrerenderingOnIOThread(child_route_id_pair.first,
                                  child_route_id_pair.second));

  // Cancel or resume all throttled resources.
  ResourceThrottleMap::iterator resource_throttle_map_it =
      resource_throttle_io_thread_map_.find(child_route_id_pair);
  DCHECK(resource_throttle_map_it != resource_throttle_io_thread_map_.end());
  ResourceThrottleList& throttles = resource_throttle_map_it->second;
  for (size_t i = 0; i < throttles.size(); i++) {
    if (throttles[i]) {
      if (final_status == FINAL_STATUS_USED) {
        throttles[i]->Resume();
      } else {
        throttles[i]->Cancel();
      }
    }
  }
  resource_throttle_io_thread_map_.erase(resource_throttle_map_it);
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

// static
PrerenderTracker* PrerenderTracker::GetDefault() {
  return g_browser_process->prerender_tracker();
}

}  // namespace prerender
