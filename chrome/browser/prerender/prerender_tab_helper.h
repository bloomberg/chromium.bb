// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace prerender {

class PrerenderManager;

// Notifies the PrerenderManager with the events happening in the prerendered
// WebContents.
class PrerenderTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PrerenderTabHelper> {
 public:
  ~PrerenderTabHelper() override;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Called when this prerendered WebContents has just been swapped in.
  void PrerenderSwappedIn();

  base::TimeTicks swap_ticks() const { return swap_ticks_; }

 private:
  explicit PrerenderTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderTabHelper>;

  // Retrieves the PrerenderManager, or NULL, if none was found.
  PrerenderManager* MaybeGetPrerenderManager() const;

  // Returns the current TimeTicks synchronized with PrerenderManager ticks. In
  // tests the clock can be mocked out in PrerenderManager, but in production
  // this should be always TimeTicks::Now().
  base::TimeTicks GetTimeTicksFromPrerenderManager() const;

  // Returns whether the WebContents being observed is currently prerendering.
  bool IsPrerendering();

  // Record the most recent swap time.
  base::TimeTicks swap_ticks_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PrerenderTabHelper);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_
