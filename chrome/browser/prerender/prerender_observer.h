// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_OBSERVER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_OBSERVER_H_

#include "content/browser/tab_contents/tab_contents_observer.h"

#include "base/time.h"

class TabContents;
class GURL;

namespace prerender {

class PrerenderContents;
class PrerenderManager;

// PrerenderObserver is responsible for recording perceived pageload times
// to compare PLT's with prerendering enabled and disabled.
class PrerenderObserver : public TabContentsObserver {
 public:
  explicit PrerenderObserver(TabContents* tab_contents);
  virtual ~PrerenderObserver();

  // TabContentsObserver implementation.
  virtual void ProvisionalChangeToMainFrameUrl(const GURL& url,
                                               bool has_opener_set) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;

  // Message handler.
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         bool main_frame,
                                         bool has_opener_set,
                                         const GURL& url);

  // Called when this prerendered TabContents has just been swapped in.
  void PrerenderSwappedIn();

 private:
  // Retrieves the PrerenderManager, or NULL, if none was found.
  PrerenderManager* MaybeGetPrerenderManager();

  // Checks with the PrerenderManager if the specified URL has been preloaded,
  // and if so, swap the RenderViewHost with the preload into this TabContents
  // object.
  bool MaybeUsePreloadedPage(const GURL& url, bool has_opener_set);

  // Returns whether the TabContents being observed is currently prerendering.
  bool IsPrerendering();

  // System time at which the current load was started for the purpose of
  // the perceived page load time (PPLT).
  base::TimeTicks pplt_load_start_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderObserver);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_OBSERVER_H_
