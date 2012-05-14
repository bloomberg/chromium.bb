// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_

#include "base/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"

class TabContentsWrapper;

namespace prerender {

class PrerenderManager;

// PrerenderTabHelper is responsible for recording perceived pageload times
// to compare PLT's with prerendering enabled and disabled.
class PrerenderTabHelper : public content::WebContentsObserver {
 public:
  explicit PrerenderTabHelper(TabContentsWrapper* tab);
  virtual ~PrerenderTabHelper();

  // content::WebContentsObserver implementation.
  virtual void ProvisionalChangeToMainFrameUrl(
      const GURL& url,
      const GURL& opener_url) OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      content::PageTransition transition_type) OVERRIDE;
  // Called when this prerendered TabContents has just been swapped in.
  void PrerenderSwappedIn();

 private:
  // Retrieves the PrerenderManager, or NULL, if none was found.
  PrerenderManager* MaybeGetPrerenderManager() const;

  // Returns whether the WebContents being observed is currently prerendering.
  bool IsPrerendering();

  bool IsTopSite(const GURL& url);

  // TabContentsWrapper we're created for.
  TabContentsWrapper* tab_;

  // System time at which the current load was started for the purpose of
  // the perceived page load time (PPLT).
  base::TimeTicks pplt_load_start_;

  // System time at which the actual pageload started (pre-swapin), if
  // a applicable (in cases when a prerender that was still loading was
  // swapped in).
  base::TimeTicks actual_load_start_;

  // Current URL being loaded.
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTabHelper);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_
