// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_observer.h"

#include "base/time.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace prerender {

PrerenderObserver::PrerenderObserver(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      pplt_load_start_() {
}

PrerenderObserver::~PrerenderObserver() {
}

void PrerenderObserver::OnProvisionalChangeToMainFrameUrl(const GURL& url) {
  PrerenderManager* pm = MaybeGetPrerenderManager();
  if (pm)
    pm->MarkTabContentsAsNotPrerendered(tab_contents());
  MaybeUsePreloadedPage(url);
}

bool PrerenderObserver::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderObserver, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartProvisionalLoadForFrame,
                        OnDidStartProvisionalLoadForFrame)
  IPC_END_MESSAGE_MAP()
  return false;
}

void PrerenderObserver::OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                                             bool is_main_frame,
                                                             const GURL& url) {
  if (is_main_frame) {
    // Record the beginning of a new PPLT navigation.
    pplt_load_start_ = base::TimeTicks::Now();
  }
}

void PrerenderObserver::DidStopLoading() {
  // Compute the PPLT metric and report it in a histogram, if needed.
  if (!pplt_load_start_.is_null()) {
    PrerenderManager::RecordPerceivedPageLoadTime(
        base::TimeTicks::Now() - pplt_load_start_);
  }

  // Reset the PPLT metric.
  pplt_load_start_ = base::TimeTicks();
}

PrerenderManager* PrerenderObserver::MaybeGetPrerenderManager() {
  return tab_contents()->profile()->GetPrerenderManager();
}

bool PrerenderObserver::MaybeUsePreloadedPage(const GURL& url) {
  PrerenderManager* pm = MaybeGetPrerenderManager();
  if (pm && pm->MaybeUsePreloadedPage(tab_contents(), url))
    return true;
  return false;
}

}  // namespace prerender
