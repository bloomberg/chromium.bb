// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_plt_recorder.h"

#include "base/time.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace prerender {

PrerenderPLTRecorder::PrerenderPLTRecorder(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      pplt_load_start_() {
}

PrerenderPLTRecorder::~PrerenderPLTRecorder() {
}

bool PrerenderPLTRecorder::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderPLTRecorder, message)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartProvisionalLoadForFrame,
                          OnDidStartProvisionalLoadForFrame)
  IPC_END_MESSAGE_MAP()
  return false;
}

void PrerenderPLTRecorder::OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                                             bool is_main_frame,
                                                             const GURL& url) {
  if (is_main_frame) {
    // Record the beginning of a new PPLT navigation.
    pplt_load_start_ = base::TimeTicks::Now();
  }
}

void PrerenderPLTRecorder::DidStopLoading() {
  // Compute the PPLT metric and report it in a histogram, if needed.
  PrerenderManager* pm = tab_contents()->profile()->GetPrerenderManager();
  if (pm != NULL && !pplt_load_start_.is_null())
    pm->RecordPerceivedPageLoadTime(base::TimeTicks::Now() - pplt_load_start_);

  // Reset the PPLT metric.
  pplt_load_start_ = base::TimeTicks();
}

}  // namespace prerender
