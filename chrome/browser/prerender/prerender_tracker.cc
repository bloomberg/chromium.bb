// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tracker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"

using content::BrowserThread;

namespace prerender {

namespace {

void CancelDeferredRequestOnIOThread(
    ResourceDispatcherHost* resource_dispatcher_host,
    int child_id, int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  resource_dispatcher_host->CancelRequest(child_id, request_id, false);
}

void StartDeferredRequestOnIOThread(
    ResourceDispatcherHost* resource_dispatcher_host,
    int child_id, int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  resource_dispatcher_host->StartDeferredRequest(child_id, request_id);
}

bool ShouldCancelRequest(
    int child_id,
    int route_id) {
  // Check if the RenderViewHost associated with (child_id, route_id) no
  // longer exists, or has already been swapped out for a prerendered page.
  // If that happens, then we do not want to issue the request which originated
  // from it. Otherwise, keep it going.
  // The RenderViewHost may exist for a couple of different reasons: such as
  // being an XHR from the originating page, being included as an iframe,
  // being requested as a favicon or stylesheet, and many other corner cases.
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(child_id, route_id);
  if (!render_view_host)
    return true;
  PrerenderManager* prerender_manager =
      FindPrerenderManagerUsingRenderProcessId(child_id);
  return (prerender_manager &&
          prerender_manager->IsOldRenderViewHost(render_view_host));
}

void HandleDelayedRequestOnUIThread(
    int child_id,
    int route_id,
    int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ResourceDispatcherHost* resource_dispatcher_host =
      g_browser_process->resource_dispatcher_host();
  CHECK(resource_dispatcher_host);
  if (ShouldCancelRequest(child_id, route_id)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CancelDeferredRequestOnIOThread, resource_dispatcher_host,
                   child_id, request_id));
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&StartDeferredRequestOnIOThread, resource_dispatcher_host,
                   child_id, request_id));
  }
}

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


void AddURL(const GURL& url, URLCounter* counter) {
  DCHECK(counter);
  counter->AddURL(url);
}

void RemoveURLs(const std::vector<GURL>& urls, URLCounter* counter) {
  DCHECK(counter);
  counter->RemoveURLs(urls);
}

}  // namespace

URLCounter::URLCounter() {
  // URLCounter is currently constructed on the UI thread but
  // accessed on the IO thread.
  DetachFromThread();
}

URLCounter::~URLCounter() {
  // URLCounter is currently destructed on the UI thread but
  // accessed on the IO thread.
  DetachFromThread();
}

bool URLCounter::MatchesURL(const GURL& url) const {
  DCHECK(CalledOnValidThread());
  URLCountMap::const_iterator it = url_count_map_.find(url);
  if (it == url_count_map_.end())
    return false;
  DCHECK(it->second > 0);
  return true;
}

void URLCounter::AddURL(const GURL& url) {
  DCHECK(CalledOnValidThread());
  URLCountMap::iterator it = url_count_map_.find(url);
  if (it == url_count_map_.end())
    url_count_map_[url] = 1;
  else
    it->second++;
}

void URLCounter::RemoveURLs(const std::vector<GURL>& urls) {
  DCHECK(CalledOnValidThread());
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end();
       ++it) {
    URLCountMap::iterator map_entry = url_count_map_.find(*it);
    DCHECK(url_count_map_.end() != map_entry);
    DCHECK(map_entry->second > 0);
    --map_entry->second;
    if (map_entry->second == 0)
      url_count_map_.erase(map_entry);
  }
}

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

bool PrerenderTracker::PotentiallyDelayRequestOnIOThread(
    const GURL& gurl,
    int process_id,
    int route_id,
    int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!url_counter_.MatchesURL(gurl))
    return false;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HandleDelayedRequestOnUIThread, process_id, route_id,
                 request_id));
  return true;
}

void PrerenderTracker::AddPrerenderURLOnUIThread(const GURL& url) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(&AddURL, url, &url_counter_));
}

void PrerenderTracker::RemovePrerenderURLsOnUIThread(
    const std::vector<GURL>& urls) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RemoveURLs, urls, &url_counter_));
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
      base::Bind(&AddPrerenderOnIOThreadTask, child_route_id_pair));

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
      base::Bind(&RemovePrerenderOnIOThreadTask, child_route_id_pair));

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
    const ChildRouteIdPair& child_route_id_pair) {
  GetDefault()->RemovePrerenderOnIOThread(child_route_id_pair);
}

}  // namespace prerender
