// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tracker.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "content/browser/browser_thread.h"
#include "content/browser/resource_context.h"
#include "content/common/resource_messages.h"
#include "net/base/load_flags.h"

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
}

PrerenderTracker::~PrerenderTracker() {
}

bool PrerenderTracker::TryUse(int child_id, int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  DCHECK(final_status >= 0 && final_status < FINAL_STATUS_MAX);

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
  if (final_status_map_.end() == final_status_map_.find(child_route_id_pair))
    return false;
  *final_status = final_status_it->second.final_status;
  return true;
}

void PrerenderTracker::OnPrerenderingStarted(
    int child_id, int route_id, PrerenderManager* prerender_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(child_id, 0);
  DCHECK_GE(route_id, 0);

  ChildRouteIdPair child_route_id_pair(child_id, route_id);

  // The RenderView should not already be prerendering.
  DCHECK(final_status_map_.end() ==
         final_status_map_.find(child_route_id_pair));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(&AddPrerenderOnIOThreadTask, child_route_id_pair));

  base::AutoLock lock(final_status_map_lock_);

  final_status_map_.insert(
      std::make_pair(child_route_id_pair, RenderViewInfo(prerender_manager)));
}

void PrerenderTracker::OnPrerenderingFinished(int child_id, int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(child_id, 0);
  DCHECK_GE(route_id, 0);

  ChildRouteIdPair child_route_id_pair(child_id, route_id);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(&RemovePrerenderOnIOThreadTask, child_route_id_pair));

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
          NewRunnableFunction(&DestroyPreloadForRenderView,
                              final_status_it->second.prerender_manager,
                              child_id,
                              route_id,
                              desired_final_status));
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
  return possibly_prerendering_io_thread_set_.end() !=
         possibly_prerendering_io_thread_set_.find(child_route_id_pair);
}

void PrerenderTracker::AddPrerenderOnIOThread(
    const ChildRouteIdPair& child_route_id_pair) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!IsPrerenderingOnIOThread(child_route_id_pair.first,
                                   child_route_id_pair.second));

  possibly_prerendering_io_thread_set_.insert(child_route_id_pair);
}

void PrerenderTracker::RemovePrerenderOnIOThread(
    const ChildRouteIdPair& child_route_id_pair) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(IsPrerenderingOnIOThread(child_route_id_pair.first,
                                  child_route_id_pair.second));

  possibly_prerendering_io_thread_set_.erase(child_route_id_pair);
}

// static
void PrerenderTracker::AddPrerenderOnIOThreadTask(
    const ChildRouteIdPair& child_route_id_pair) {
  g_browser_process->prerender_tracker()->AddPrerenderOnIOThread(
      child_route_id_pair);
}

// static
void PrerenderTracker::RemovePrerenderOnIOThreadTask(
    const ChildRouteIdPair& child_route_id_pair) {
  g_browser_process->prerender_tracker()->RemovePrerenderOnIOThread(
      child_route_id_pair);
}

}  // namespace prerender
