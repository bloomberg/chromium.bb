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

namespace {

void DestroyPrerenderForRenderViewOnUI(
    const base::WeakPtr<PrerenderManager>& prerender_manager_weak_ptr,
    int render_process_id,
    int render_view_id,
    FinalStatus final_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrerenderManager* prerender_manager = prerender_manager_weak_ptr.get();
  if (!prerender_manager)
    return;

  prerender_manager->DestroyPrerenderForRenderView(
      render_process_id, render_view_id, final_status);
}

}  // namespace

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
  DCHECK(final_status_map_.empty());
}

bool PrerenderTracker::TryUse(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  return SetFinalStatus(child_id, route_id, FINAL_STATUS_USED, NULL);
}

bool PrerenderTracker::TryCancel(
    int child_id,
    int route_id,
    FinalStatus final_status) {
  DCHECK_NE(FINAL_STATUS_USED, final_status);
  DCHECK(final_status >= 0 && final_status < FINAL_STATUS_MAX);

  FinalStatus actual_final_status;
  SetFinalStatus(child_id, route_id, final_status, &actual_final_status);
  return actual_final_status != FINAL_STATUS_USED &&
         actual_final_status != FINAL_STATUS_MAX;
}

bool PrerenderTracker::TryCancelOnIOThread(
    int child_id,
    int route_id,
    FinalStatus final_status) {
  DCHECK_NE(FINAL_STATUS_USED, final_status);
  DCHECK_LE(0, final_status);
  DCHECK_GT(FINAL_STATUS_MAX, final_status);

  if (!IsPrerenderingOnIOThread(child_id, route_id))
    return false;
  return TryCancel(child_id, route_id, final_status);
}

bool PrerenderTracker::GetFinalStatus(int child_id, int route_id,
                                      FinalStatus* final_status) const {
  ChildRouteIdPair child_route_id_pair(child_id, route_id);

  base::AutoLock lock(final_status_map_lock_);
  FinalStatusMap::const_iterator final_status_it =
      final_status_map_.find(child_route_id_pair);
  if (final_status_it == final_status_map_.end())
    return false;
  *final_status = final_status_it->second.final_status;
  return true;
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
      base::Bind(&AddPrerenderOnIOThreadTask, child_route_id_pair));

  base::AutoLock lock(final_status_map_lock_);
  // The RenderView should not already be prerendering.
  DCHECK_EQ(0u, final_status_map_.count(child_route_id_pair));

  final_status_map_.insert(
      std::make_pair(child_route_id_pair,
                     RenderViewInfo(prerender_contents->prerender_manager())));
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
      base::Bind(&RemovePrerenderOnIOThreadTask, child_route_id_pair,
                 prerender_contents->final_status()));

  base::AutoLock lock(final_status_map_lock_);
  size_t num_erased = final_status_map_.erase(child_route_id_pair);
  DCHECK_EQ(1u, num_erased);
}

bool PrerenderTracker::SetFinalStatus(int child_id, int route_id,
                                      FinalStatus desired_final_status,
                                      FinalStatus* actual_final_status) {
  DCHECK(desired_final_status >= FINAL_STATUS_USED &&
         desired_final_status < FINAL_STATUS_MAX);

  ChildRouteIdPair child_route_id_pair(child_id, route_id);

  base::AutoLock lock(final_status_map_lock_);
  FinalStatusMap::iterator final_status_it =
      final_status_map_.find(child_route_id_pair);
  if (final_status_it == final_status_map_.end()) {
    // The RenderView has already been either used or destroyed.
    if (actual_final_status)
      *actual_final_status = FINAL_STATUS_MAX;
    return false;
  }

  if (final_status_it->second.final_status == FINAL_STATUS_MAX) {
    final_status_it->second.final_status = desired_final_status;
    if (desired_final_status != FINAL_STATUS_USED) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&DestroyPrerenderForRenderViewOnUI,
                     final_status_it->second.prerender_manager, child_id,
                     route_id, desired_final_status));
    }

    if (actual_final_status)
      *actual_final_status = desired_final_status;
    return true;
  }

  if (actual_final_status)
    *actual_final_status = final_status_it->second.final_status;
  return false;
}

bool PrerenderTracker::IsPrerenderingOnIOThread(int child_id,
                                                int route_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ChildRouteIdPair child_route_id_pair(child_id, route_id);
  return resource_throttle_io_thread_map_.count(child_route_id_pair) > 0;
}

bool PrerenderTracker::IsPendingSwapRequestOnIOThread(
    int child_id, int route_id, const GURL& url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ChildRouteIdPair child_route_id_pair(child_id, route_id);
  PendingSwapThrottleMap::const_iterator it =
      pending_swap_throttle_map_.find(child_route_id_pair);
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
    int child_id,
    int route_id,
    const GURL& url,
    const base::WeakPtr<PrerenderPendingSwapThrottle>& throttle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ChildRouteIdPair child_route_id_pair(child_id, route_id);
  PendingSwapThrottleMap::iterator it =
      pending_swap_throttle_map_.find(child_route_id_pair);
  DCHECK(it != pending_swap_throttle_map_.end());
  if (it == pending_swap_throttle_map_.end())
    return;
  it->second.throttles.push_back(throttle);
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
    const ChildRouteIdPair& child_route_id_pair,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::pair<PendingSwapThrottleMap::iterator, bool> insert_result =
      pending_swap_throttle_map_.insert(std::make_pair(
          child_route_id_pair, PendingSwapThrottleData(url)));
  DCHECK(insert_result.second);
}

void PrerenderTracker::RemovePrerenderPendingSwapOnIOThread(
    const ChildRouteIdPair& child_route_id_pair,
    bool swap_successful) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  PendingSwapThrottleMap::iterator it =
      pending_swap_throttle_map_.find(child_route_id_pair);
  DCHECK(it != pending_swap_throttle_map_.end());
  // Cancel or resume all throttled resources.
  for (size_t i = 0; i < it->second.throttles.size(); i++) {
    if (!it->second.throttles[i])
      continue;
    if (swap_successful)
      it->second.throttles[i]->Cancel();
    else
      it->second.throttles[i]->Resume();
  }
  pending_swap_throttle_map_.erase(child_route_id_pair);
}

void PrerenderTracker::AddPrerenderPendingSwap(
    const ChildRouteIdPair& child_route_id_pair,
    const GURL& url) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AddPrerenderPendingSwapOnIOThreadTask,
                 child_route_id_pair, url));
}

void PrerenderTracker::RemovePrerenderPendingSwap(
    const ChildRouteIdPair& child_route_id_pair,
    bool swap_successful) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RemovePrerenderPendingSwapOnIOThreadTask,
                 child_route_id_pair, swap_successful));
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

// static
void PrerenderTracker::AddPrerenderOnIOThreadTask(
    const ChildRouteIdPair& child_route_id_pair) {
  GetDefault()->AddPrerenderOnIOThread(child_route_id_pair);
}

// static
void PrerenderTracker::RemovePrerenderOnIOThreadTask(
    const ChildRouteIdPair& child_route_id_pair,
    FinalStatus final_status) {
  GetDefault()->RemovePrerenderOnIOThread(child_route_id_pair, final_status);
}

// static
void PrerenderTracker::AddPrerenderPendingSwapOnIOThreadTask(
    const ChildRouteIdPair& child_route_id_pair,
    const GURL& url) {
  GetDefault()->AddPrerenderPendingSwapOnIOThread(child_route_id_pair, url);
}

// static
void PrerenderTracker::RemovePrerenderPendingSwapOnIOThreadTask(
    const ChildRouteIdPair& child_route_id_pair,
    bool swap_successful) {
  GetDefault()->RemovePrerenderPendingSwapOnIOThread(child_route_id_pair,
                                                     swap_successful);
}

}  // namespace prerender
