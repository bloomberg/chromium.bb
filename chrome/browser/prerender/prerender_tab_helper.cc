// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tab_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(prerender::PrerenderTabHelper);

namespace prerender {

PrerenderTabHelper::PrerenderTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      origin_(ORIGIN_NONE),
      weak_factory_(this) {}

PrerenderTabHelper::~PrerenderTabHelper() {
}

void PrerenderTabHelper::DidGetRedirectForResourceRequest(
    const content::ResourceRedirectDetails& details) {
  if (details.resource_type != content::RESOURCE_TYPE_MAIN_FRAME)
    return;

  MainFrameUrlDidChange(details.new_url);
}

void PrerenderTabHelper::DidFinishNavigation(
      content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

  url_ = navigation_handle->GetURL();
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents(), NULL))
    return;
  prerender_manager->RecordNavigation(url_);
}

void PrerenderTabHelper::DidStopLoading() {
  // Compute the PPLT metric and report it in a histogram, if needed. If the
  // page is still prerendering, record the not swapped in page load time
  // instead.
  if (!pplt_load_start_.is_null()) {
    base::TimeTicks now = GetTimeTicksFromPrerenderManager();
    if (IsPrerendering()) {
      PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
      if (prerender_manager) {
        prerender_manager->RecordPageLoadTimeNotSwappedIn(
            origin_, now - pplt_load_start_, url_);
      } else {
        NOTREACHED();
      }
    } else {
      double fraction_elapsed_at_swapin = -1.0;
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

      RecordPerceivedPageLoadTime(
          now - pplt_load_start_, fraction_elapsed_at_swapin);
    }
  }

  // Reset the PPLT metric.
  pplt_load_start_ = base::TimeTicks();
  actual_load_start_ = base::TimeTicks();
}

void PrerenderTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSamePage())
    return;

  // Determine the navigation type.
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents(), &origin_)) {
    navigation_type_ = NAVIGATION_TYPE_PRERENDERED;
  } else {
    navigation_type_ = NAVIGATION_TYPE_NORMAL;
  }

  if (!navigation_handle->IsInMainFrame())
    return;

  // Record PPLT state for the beginning of a new navigation.
  pplt_load_start_ = GetTimeTicksFromPrerenderManager();
  actual_load_start_ = base::TimeTicks();

  MainFrameUrlDidChange(navigation_handle->GetURL());
}

void PrerenderTabHelper::MainFrameUrlDidChange(const GURL& url) {
  url_ = url;
}

PrerenderManager* PrerenderTabHelper::MaybeGetPrerenderManager() const {
  return PrerenderManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

base::TimeTicks PrerenderTabHelper::GetTimeTicksFromPrerenderManager() const {
  // Prerender browser tests should always have a PrerenderManager when mocking
  // out tick clock.
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (prerender_manager)
    return prerender_manager->GetCurrentTimeTicks();

  // Fall back to returning the same value as PrerenderManager would have
  // returned in production.
  return base::TimeTicks::Now();
}

bool PrerenderTabHelper::IsPrerendering() {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return false;
  return prerender_manager->IsWebContentsPrerendering(web_contents(), NULL);
}

void PrerenderTabHelper::PrerenderSwappedIn() {
  // Ensure we are not prerendering any more.
  DCHECK_EQ(NAVIGATION_TYPE_PRERENDERED, navigation_type_);
  DCHECK(!IsPrerendering());
  swap_ticks_ = GetTimeTicksFromPrerenderManager();
  if (pplt_load_start_.is_null()) {
    // If we have already finished loading, report a 0 PPLT.
    RecordPerceivedPageLoadTime(base::TimeDelta(), 1.0);
  } else {
    // If we have not finished loading yet, record the actual load start, and
    // rebase the start time to now.
    actual_load_start_ = pplt_load_start_;
    pplt_load_start_ = GetTimeTicksFromPrerenderManager();
  }
}

void PrerenderTabHelper::RecordPerceivedPageLoadTime(
    base::TimeDelta perceived_page_load_time,
    double fraction_plt_elapsed_at_swap_in) {
  DCHECK(!IsPrerendering());
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;

  prerender_manager->RecordPerceivedPageLoadTime(
      origin_, navigation_type_, perceived_page_load_time,
      fraction_plt_elapsed_at_swap_in, url_);
}

}  // namespace prerender
