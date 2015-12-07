// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"

namespace page_load_metrics {

struct PageLoadExtraInfo {
  PageLoadExtraInfo(const base::TimeDelta& first_background_time,
                    const base::TimeDelta& first_foreground_time,
                    bool started_in_foreground,
                    bool has_commit);

  // Returns the time to first background if the page load started in the
  // foreground. If the page has not been backgrounded, or the page started in
  // the background, this will be base::TimeDelta().
  const base::TimeDelta first_background_time;

  // Returns the time to first foreground if the page load started in the
  // background. If the page has not been foregrounded, or the page started in
  // the foreground, this will be base::TimeDelta().
  const base::TimeDelta first_foreground_time;

  // True if the page load started in the foreground.
  const bool started_in_foreground;

  // True if the page load committed and received its first bytes of data.
  const bool has_commit;
};

// Interface for PageLoadMetrics observers. All instances of this class are
// owned by the PageLoadTracker tracking a page load. They will be deleted after
// calling OnComplete.
class PageLoadMetricsObserver {
 public:
  virtual ~PageLoadMetricsObserver() {}

  // The page load started, with the given navigation handle.
  virtual void OnStart(content::NavigationHandle* navigation_handle) {}

  // OnRedirect is triggered when a page load redirects to another URL.
  // The navigation handle holds relevant data for the navigation, but will
  // be destroyed soon after this call. Don't hold a reference to it. This can
  // be called multiple times.
  virtual void OnRedirect(content::NavigationHandle* navigation_handle) {}

  // OnCommit is triggered when a page load commits, i.e. when we receive the
  // first data for the request. The navigation handle holds relevant data for
  // the navigation, but will be destroyed soon after this call. Don't hold a
  // reference to it.
  virtual void OnCommit(content::NavigationHandle* navigation_handle) {}

  // OnComplete is triggered when we are ready to record metrics for this page
  // load. This will happen some time after commit. The PageLoadTiming struct
  // contains timing data and the PageLoadExtraInfo struct contains other useful
  // data collected over the course of the page load. If the load did not
  // receive any timing information, |timing.IsEmpty()| will be true.
  // After this call, the object will be deleted.
  virtual void OnComplete(const PageLoadTiming& timing,
                          const PageLoadExtraInfo& extra_info) {}
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
