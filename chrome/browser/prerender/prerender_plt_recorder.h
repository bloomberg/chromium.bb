// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_PLT_RECORDER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_PLT_RECORDER_H_
#pragma once

#include <list>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/tab_contents/tab_contents_observer.h"
#include "googleurl/src/gurl.h"

class PrerenderContents;
class Profile;
class TabContents;

namespace prerender {

// PrerenderPLTRecorder is responsible for recording perceived pageload times
// to compare PLT's with prerendering enabled and disabled.
class PrerenderPLTRecorder : public TabContentsObserver {
 public:
  explicit PrerenderPLTRecorder(TabContents* tab_contents);
  virtual ~PrerenderPLTRecorder();

  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Message handler.
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         bool main_frame,
                                         const GURL& url);

  virtual void DidStopLoading();

 private:
  TabContents* tab_contents_;

  // System time at which the current load was started for the purpose of
  // the perceived page load time (PPLT).
  base::TimeTicks pplt_load_start_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderPLTRecorder);
};

}  // prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_PLT_RECORDER_H_
