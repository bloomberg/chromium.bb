// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include "base/logging.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"
#include "chrome/common/render_messages.h"

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
    delete data.contents_;
  }
}

void PrerenderManager::SetPrerenderContentsFactory(
    PrerenderContents::Factory* prerender_contents_factory) {
  prerender_contents_factory_.reset(prerender_contents_factory);
}

void PrerenderManager::AddPreload(const GURL& url,
                                  const std::vector<GURL>& alias_urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeleteOldEntries();
  // If the URL already exists in the set of preloaded URLs, don't do anything.
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->url_ == url)
      return;
  }
  PrerenderContentsData data(CreatePrerenderContents(url, alias_urls),
                             GetCurrentTime(), url);
  prerender_list_.push_back(data);
  data.contents_->StartPrerendering();
  while (prerender_list_.size() > max_elements_) {
    data = prerender_list_.front();
    prerender_list_.pop_front();
    delete data.contents_;
  }
}

void PrerenderManager::DeleteOldEntries() {
  while (prerender_list_.size() > 0) {
    PrerenderContentsData data = prerender_list_.front();
    if (IsPrerenderElementFresh(data.start_time_))
      return;
    prerender_list_.pop_front();
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
  delete entry;
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
    const std::vector<GURL>& alias_urls) {
  return prerender_contents_factory_->CreatePrerenderContents(
      this, profile_, url, alias_urls);
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
