// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tab_helper.h"

#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"

namespace prerender {

namespace {

enum PAGEVIEW_EVENTS {
  PAGEVIEW_EVENT_NEW_URL = 0,
  PAGEVIEW_EVENT_TOP_SITE_NEW_URL = 1,
  PAGEVIEW_EVENT_LOAD_START = 2,
  PAGEVIEW_EVENT_TOP_SITE_LOAD_START = 3,
  PAGEVIEW_EVENT_MAX = 4
};

void RecordPageviewEvent(PAGEVIEW_EVENTS event) {
  UMA_HISTOGRAM_ENUMERATION("Prerender.PageviewEvents",
                            event, PAGEVIEW_EVENT_MAX);
}

}  // namespace

PrerenderTabHelper::PrerenderTabHelper(TabContentsWrapper* tab)
    : content::WebContentsObserver(tab->web_contents()),
      tab_(tab) {
}

PrerenderTabHelper::~PrerenderTabHelper() {
}

void PrerenderTabHelper::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    const GURL& opener_url) {
  url_ = url;
  RecordPageviewEvent(PAGEVIEW_EVENT_NEW_URL);
  if (IsTopSite(url))
    RecordPageviewEvent(PAGEVIEW_EVENT_TOP_SITE_NEW_URL);
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents()))
    return;
  prerender_manager->MarkWebContentsAsNotPrerendered(web_contents());
}

void PrerenderTabHelper::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    content::PageTransition transition_type) {
  if (!is_main_frame)
    return;
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents()))
    return;
  prerender_manager->RecordNavigation(validated_url);
}

void PrerenderTabHelper::DidStopLoading() {
  // Compute the PPLT metric and report it in a histogram, if needed.
  // We include pages that are still prerendering and have just finished
  // loading -- PrerenderManager will sort this out and handle it correctly
  // (putting those times into a separate histogram).
  if (!pplt_load_start_.is_null()) {
    double fraction_elapsed_at_swapin = -1.0;
    base::TimeTicks now = base::TimeTicks::Now();
    if (!actual_load_start_.is_null()) {
      double plt = (now - actual_load_start_).InMillisecondsF();
      if (plt > 0.0) {
        fraction_elapsed_at_swapin = 1.0 -
            (now - pplt_load_start_).InMillisecondsF() / plt;
      } else {
        fraction_elapsed_at_swapin = 1.0;
      }
      DCHECK_GE(fraction_elapsed_at_swapin, 0.0);
      DCHECK_LE(fraction_elapsed_at_swapin, 1.0);
    }
    PrerenderManager::RecordPerceivedPageLoadTime(
        now - pplt_load_start_, fraction_elapsed_at_swapin, web_contents(),
        url_);
  }

  // Reset the PPLT metric.
  pplt_load_start_ = base::TimeTicks();
  actual_load_start_ = base::TimeTicks();
}

void PrerenderTabHelper::DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      content::RenderViewHost* render_view_host) {
  if (is_main_frame) {
    RecordPageviewEvent(PAGEVIEW_EVENT_LOAD_START);
    if (IsTopSite(validated_url))
      RecordPageviewEvent(PAGEVIEW_EVENT_TOP_SITE_LOAD_START);

    // Record the beginning of a new PPLT navigation.
    pplt_load_start_ = base::TimeTicks::Now();
    actual_load_start_ = base::TimeTicks();
  }
}

PrerenderManager* PrerenderTabHelper::MaybeGetPrerenderManager() const {
  return PrerenderManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

bool PrerenderTabHelper::IsPrerendering() {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return false;
  return prerender_manager->IsWebContentsPrerendering(web_contents());
}

void PrerenderTabHelper::PrerenderSwappedIn() {
  // Ensure we are not prerendering any more.
  DCHECK(!IsPrerendering());
  if (pplt_load_start_.is_null()) {
    // If we have already finished loading, report a 0 PPLT.
    PrerenderManager::RecordPerceivedPageLoadTime(base::TimeDelta(), 1.0,
                                                  web_contents(), url_);
  } else {
    // If we have not finished loading yet, record the actual load start, and
    // rebase the start time to now.
    actual_load_start_ = pplt_load_start_;
    pplt_load_start_ = base::TimeTicks::Now();
  }
}

bool PrerenderTabHelper::IsTopSite(const GURL& url) {
  PrerenderManager* pm = MaybeGetPrerenderManager();
  return (pm && pm->IsTopSite(url));
}

}  // namespace prerender
