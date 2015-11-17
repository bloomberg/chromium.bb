// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"

namespace page_load_metrics {

struct PageLoadExtraInfo {
  PageLoadExtraInfo(const base::TimeDelta& first_background_time,
                    const base::TimeDelta& first_foreground_time,
                    bool started_in_foreground);

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
};

// Interface for PageLoadMetrics observers. Note that it might be possible for
// OnCommit to be fired without a subsequent OnComplete (i.e. if an error
// occurs or we don't have any metrics to log). PageLoadMetricsObservers are
// required to stop observing (call PageLoadMetricsObservable::RemoveObserver)
// some time before the OnPageLoadMetricsGoingAway trigger finishes. If this
// class is destroyed before the PageLoadMetricsObservable, it should call
// RemoveObserver in its destructor.
class PageLoadMetricsObserver {
 public:
  virtual ~PageLoadMetricsObserver() {}

  // OnRedirect is triggered when a page load redirects to another URL.
  // The navigation handle holds relevant data for the navigation, but will
  // be destroyed soon after this call. Don't hold a reference to it.
  virtual void OnRedirect(content::NavigationHandle* navigation_handle) {}

  // OnCommit is triggered when a page load commits, i.e. when we receive the
  // first data for the request. The navigation handle holds relevant data for
  // the navigation, but will be destroyed soon after this call. Don't hold a
  // reference to it.
  virtual void OnCommit(content::NavigationHandle* navigation_handle) {}

  // OnComplete is triggered when we are ready to record metrics for this page
  // load. This will happen some time after commit, and will be triggered as
  // long as we've received some data about the page load. The
  // PageLoadTiming struct contains timing data and the PageLoadExtraInfo struct
  // contains other useful information about the tab backgrounding/foregrounding
  // over the course of the page load.
  virtual void OnComplete(const PageLoadTiming& timing,
                          const PageLoadExtraInfo& extra_info) {}

  // This is called when the WebContents we are observing is tearing down. No
  // further callbacks will be triggered.
  virtual void OnPageLoadMetricsGoingAway() {}
};

// Class which handles notifying observers when loads commit and complete. It
// must call OnPageLoadMetricsGoingAway in the destructor.
class PageLoadMetricsObservable {
 public:
  virtual ~PageLoadMetricsObservable() {}
  virtual void AddObserver(PageLoadMetricsObserver* observer) = 0;
  virtual void RemoveObserver(PageLoadMetricsObserver* observer) = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
