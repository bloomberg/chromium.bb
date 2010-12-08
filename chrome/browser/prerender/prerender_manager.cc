// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager.h"

#include "base/logging.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"

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
    : profile_(profile) {
}

PrerenderManager::~PrerenderManager() {
  while (prerender_list_.size() > 0) {
    PrerenderContentsData data = prerender_list_.front();
    prerender_list_.pop_front();
    delete data.contents_;
  }
}

void PrerenderManager::AddPreload(const GURL& url) {
  DCHECK(CalledOnValidThread());
  DeleteOldEntries();
  // If the URL already exists in the set of preloaded URLs, don't do anything.
  for (std::list<PrerenderContentsData>::iterator it = prerender_list_.begin();
       it != prerender_list_.end();
       ++it) {
    if (it->url_ == url)
      return;
  }
  PrerenderContentsData data(new PrerenderContents(this, profile_, url),
                             base::Time::Now(), url);
  prerender_list_.push_back(data);
  data.contents_->StartPrerendering();
  while (prerender_list_.size() > kMaxPrerenderElements) {
    data = prerender_list_.front();
    prerender_list_.pop_front();
    delete data.contents_;
  }
}

void PrerenderManager::DeleteOldEntries() {
  base::Time now = base::Time::Now();
  while (prerender_list_.size() > 0) {
    PrerenderContentsData data = prerender_list_.front();
    if ((now - data.start_time_).InSeconds() < kMaxPrerenderAgeSeconds)
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
    if (it->url_ == url) {
      PrerenderContents* pc = it->contents_;
      prerender_list_.erase(it);
      return pc;
    }
  }
  // Entry not found.
  return NULL;
}

bool PrerenderManager::MaybeUsePreloadedPage(TabContents* tc, const GURL& url) {
  DCHECK(CalledOnValidThread());
  scoped_ptr<PrerenderContents> pc(GetEntry(url));
  if (pc.get() == NULL)
    return false;

  RenderViewHost* rvh = pc->render_view_host();
  pc->set_render_view_host(NULL);
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
  DCHECK(CalledOnValidThread());
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
