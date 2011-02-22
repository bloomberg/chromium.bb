// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/fav_icon_helper.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"
#include "chrome/common/render_messages.h"

namespace prerender {

// static
base::TimeTicks PrerenderManager::last_prefetch_seen_time_;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::mode_ =
    PRERENDER_MODE_ENABLED;

// static
PrerenderManager::PrerenderManagerMode PrerenderManager::GetMode() {
  return mode_;
}

// static
void PrerenderManager::SetMode(PrerenderManagerMode mode) {
  mode_ = mode;
}

// static
bool PrerenderManager::IsPrerenderingEnabled() {
  return
      GetMode() == PRERENDER_MODE_ENABLED ||
      GetMode() == PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP;
}

struct PrerenderManager::PrerenderContentsData {
  PrerenderContents* contents_;
  base::Time start_time_;
  GURL url_;
  PrerenderContentsData(PrerenderContents* contents,
                        base::Time start_time,
                        GURL url)
      : contents_(contents),
        start_time_(start_time),
        url_(url) {
  }
};

PrerenderManager::PrerenderManager(Profile* profile)
    : profile_(profile),
      max_prerender_age_(base::TimeDelta::FromSeconds(
          kDefaultMaxPrerenderAgeSeconds)),
      max_elements_(kDefaultMaxPrerenderElements),
      prerender_contents_factory_(PrerenderContents::CreateFactory()) {
}

PrerenderManager::~PrerenderManager() {
  while (prerender_list_.size() > 0) {
    PrerenderContentsData data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->set_final_status(FINAL_STATUS_MANAGER_SHUTDOWN);
    delete data.contents_;
  }
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  prerender_contents_factory_.reset(prerender_contents_factory);
}

void PrerenderManager::AddPreload(const GURL& url,
                                  const std::vector<GURL>& alias_urls,
                                  const GURL& referrer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeleteOldEntries();
  // If the URL already exists in the set of preloaded URLs, don't do anything.
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->url_ == url)
      return;
  }
  PrerenderContentsData data(CreatePrerenderContents(url, alias_urls, referrer),
                             GetCurrentTime(), url);
  prerender_list_.push_back(data);
  data.contents_->StartPrerendering();
  while (prerender_list_.size() > max_elements_) {
    data = prerender_list_.front();
    prerender_list_.pop_front();
    data.contents_->set_final_status(FINAL_STATUS_EVICTED);
    delete data.contents_;
  }
}

void PrerenderManager::DeleteOldEntries() {
  while (prerender_list_.size() > 0) {
    PrerenderContentsData data = prerender_list_.front();
    if (IsPrerenderElementFresh(data.start_time_))
      return;
    prerender_list_.pop_front();
    data.contents_->set_final_status(FINAL_STATUS_TIMED_OUT);
    delete data.contents_;
  }
}

PrerenderContents* PrerenderManager::GetEntry(const GURL& url) {
  DeleteOldEntries();
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    PrerenderContents* pc = it->contents_;
    if (pc->MatchesURL(url)) {
      PrerenderContents* pc = it->contents_;
      prerender_list_.erase(it);
      return pc;
    }
  }
  // Entry not found.
  return NULL;
}

bool PrerenderManager::MaybeUsePreloadedPage(TabContents* tc, const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<PrerenderContents> pc(GetEntry(url));
  if (pc.get() == NULL)
    return false;

  if (!pc->load_start_time().is_null())
    RecordTimeUntilUsed(base::TimeTicks::Now() - pc->load_start_time());
  pc->set_final_status(FINAL_STATUS_USED);

  RenderViewHost* rvh = pc->render_view_host();
  pc->set_render_view_host(NULL);
  rvh->Send(new ViewMsg_DisplayPrerenderedPage(rvh->routing_id()));
  tc->SwapInRenderViewHost(rvh);

  ViewHostMsg_FrameNavigate_Params* p = pc->navigate_params();
  if (p != NULL)
    tc->DidNavigate(rvh, *p);

  string16 title = pc->title();
  if (!title.empty())
    tc->UpdateTitle(rvh, pc->page_id(), UTF16ToWideHack(title));

  GURL icon_url = pc->icon_url();
  if (!icon_url.is_empty())
    tc->fav_icon_helper().OnUpdateFavIconURL(pc->page_id(), icon_url);

  if (pc->has_stopped_loading())
    tc->DidStopLoading();

  tc->set_was_prerendered(true);

  return true;
}

void PrerenderManager::RemoveEntry(PrerenderContents* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_ == entry) {
      prerender_list_.erase(it);
      break;
    }
  }
  DeleteOldEntries();
}

base::Time PrerenderManager::GetCurrentTime() const {
  return base::Time::Now();
}

bool PrerenderManager::IsPrerenderElementFresh(const base::Time start) const {
  base::Time now = GetCurrentTime();
  return (now - start < max_prerender_age_);
}

PrerenderContents* PrerenderManager::CreatePrerenderContents(
    const GURL& url,
    const std::vector<GURL>& alias_urls,
    const GURL& referrer) {
  return prerender_contents_factory_->CreatePrerenderContents(
      this, profile_, url, alias_urls, referrer);
}

void PrerenderManager::RecordPerceivedPageLoadTime(base::TimeDelta pplt) {
  bool record_windowed_pplt = ShouldRecordWindowedPPLT();
  switch (mode_) {
    case PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP:
      UMA_HISTOGRAM_TIMES("Prerender.PerceivedPageLoadTime_Control", pplt);
      if (record_windowed_pplt) {
        UMA_HISTOGRAM_TIMES("Prerender.PerceivedPageLoadTime_WindowControl",
                            pplt);
      }
      break;
    case PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP:
      UMA_HISTOGRAM_TIMES("Prerender.PerceivedPageLoadTime_Treatment", pplt);
      if (record_windowed_pplt) {
        UMA_HISTOGRAM_TIMES("Prerender.PerceivedPageLoadTime_WindowTreatment",
                            pplt);
      }
      break;
    default:
      break;
  }
}

void PrerenderManager::RecordTimeUntilUsed(base::TimeDelta time_until_used) {
  if (mode_ == PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP) {
    UMA_HISTOGRAM_TIMES("Prerender.TimeUntilUsed", time_until_used);
  }
}

PrerenderContents* PrerenderManager::FindEntry(const GURL& url) {
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->contents_->MatchesURL(url))
      return it->contents_;
  }
  // Entry not found.
  return NULL;
}

void PrerenderManager::RecordPrefetchTagObserved() {
  // Ensure that we are in the UI thread, and post to the UI thread if
  // necessary.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableFunction(
            &PrerenderManager::RecordPrefetchTagObservedOnUIThread));
  } else {
    RecordPrefetchTagObservedOnUIThread();
  }
}

void PrerenderManager::RecordPrefetchTagObservedOnUIThread() {
  // Once we get here, we have to be on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If we observe multiple tags within the 30 second window, we will still
  // reset the window to begin at the most recent occurrence, so that we will
  // always be in a window in the 30 seconds from each occurrence.
  last_prefetch_seen_time_ = base::TimeTicks::Now();
}

bool PrerenderManager::ShouldRecordWindowedPPLT() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (last_prefetch_seen_time_.is_null())
    return false;
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_prefetch_seen_time_;
  return elapsed_time <= base::TimeDelta::FromSeconds(kWindowedPPLTSeconds);
}

}  // namespace prerender
