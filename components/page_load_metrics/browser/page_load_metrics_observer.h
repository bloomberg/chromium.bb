// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace page_load_metrics {

class PageLoadMetricsObservable;

// This enum represents how a page load ends. If the action occurs before the
// page load finishes (or reaches some point like first paint), then we consider
// the load to be aborted.
enum UserAbortType {
  // Represents no abort.
  ABORT_NONE,

  // If the user presses reload or shift-reload.
  ABORT_RELOAD,

  // The user presses the back/forward button.
  ABORT_FORWARD_BACK,

  // If the navigation is replaced by a new navigation. This includes link
  // clicks, typing in the omnibox (not a reload), and form submissions.
  ABORT_NEW_NAVIGATION,

  // If the user presses the stop X button.
  ABORT_STOP,

  // If the navigation is aborted by closing the tab or browser.
  ABORT_CLOSE,

  // We don't know why the navigation aborted. This is the value we assign to an
  // aborted load if the only signal we get is a provisional load finishing
  // without committing, either without error or with net::ERR_ABORTED.
  ABORT_OTHER,

  // Add values before this final count.
  ABORT_LAST_ENTRY
};

struct PageLoadExtraInfo {
  PageLoadExtraInfo(const base::TimeDelta& first_background_time,
                    const base::TimeDelta& first_foreground_time,
                    bool started_in_foreground,
                    const GURL& committed_url,
                    const base::TimeDelta& time_to_commit,
                    UserAbortType abort_type,
                    const base::TimeDelta& time_to_abort);

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

  // Committed URL. If the page load did not commit, |committed_url| will be
  // empty.
  const GURL committed_url;

  // Time from navigation start until commit. If the page load did not commit,
  // |time_to_commit| will be zero.
  const base::TimeDelta time_to_commit;

  // The abort time and time to abort for this page load. If the page was not
  // aborted, |abort_type| will be |ABORT_NONE| and |time_to_abort| will be
  // |base::TimeDelta()|.
  const UserAbortType abort_type;
  const base::TimeDelta time_to_abort;
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
  // Note that this does not get called for same page navigations.
  virtual void OnCommit(content::NavigationHandle* navigation_handle) {}

  // OnFailedProvisionalLoad is triggered when a provisional load failed and did
  // not commit. Note that provisional loads that result in downloads or 204s
  // are aborted by the system, and thus considered failed provisional loads.
  virtual void OnFailedProvisionalLoad(
      content::NavigationHandle* navigation_handle) {}

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
