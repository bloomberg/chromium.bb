// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_WEB_CONTENTS_OBSERVER_H_

#include "base/containers/scoped_ptr_map.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/net_errors.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace IPC {
class Message;
}  // namespace IPC

namespace page_load_metrics {

// If you add elements from this enum, make sure you update the enum
// value in histograms.xml. Only add elements to the end to prevent
// inconsistencies between versions.
enum PageLoadEvent {
  PAGE_LOAD_STARTED,

  // A provisional load is a load before it commits, i.e. before it receives the
  // first bytes of the response body or knows the encoding of the response
  // body. A load failed before it was committed for any reason, e.g. from a
  // user abort or a network timeout.
  PAGE_LOAD_FAILED_BEFORE_COMMIT,

  // A subset of PAGE_LOAD_FAILED_BEFORE_COMMIT, this counts the
  // specific failures due to user aborts.
  PAGE_LOAD_ABORTED_BEFORE_COMMIT,

  // When a load is aborted anytime before the page's first layout, we increase
  // these counts. This includes all failed provisional loads.
  PAGE_LOAD_ABORTED_BEFORE_FIRST_LAYOUT,

  // We increase this count if a page load successfully has a layout.
  // Differentiate between loads that were backgrounded before first layout.
  // Note that a load that is backgrounded, then foregrounded before first
  // layout will still end up in the backgrounded bucket.
  PAGE_LOAD_SUCCESSFUL_FIRST_LAYOUT_FOREGROUND,
  PAGE_LOAD_SUCCESSFUL_FIRST_LAYOUT_BACKGROUND,

  // Add values before this final count.
  PAGE_LOAD_LAST_ENTRY
};

class PageLoadTracker {
 public:
  explicit PageLoadTracker(bool in_foreground);
  ~PageLoadTracker();
  void Commit();
  void WebContentsHidden();

  // Returns true if the timing was successfully updated.
  bool UpdateTiming(const PageLoadTiming& timing);
  void RecordEvent(PageLoadEvent event);

 private:
  void RecordTimingHistograms();

  bool has_commit_;

  // We record separate metrics for events that occur after a background,
  // because metrics like layout/paint are delayed artificially
  // when they occur in the bacground.
  base::TimeTicks background_time_;
  bool started_in_foreground_;

  PageLoadTiming timing_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadTracker);
};

// MetricsWebContentsObserver logs page load UMA metrics based on
// IPC messages received from a MetricsRenderFrameObserver.
class MetricsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetricsWebContentsObserver> {
 public:
  ~MetricsWebContentsObserver() override;

  // content::WebContentsObserver implementation:
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void WasShown() override;
  void WasHidden() override;

  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  explicit MetricsWebContentsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<MetricsWebContentsObserver>;
  friend class MetricsWebContentsObserverTest;

  void OnTimingUpdated(content::RenderFrameHost*, const PageLoadTiming& timing);

  bool IsRelevantNavigation(content::NavigationHandle* navigation_handle);

  // True if the web contents is currently in the foreground.
  bool in_foreground_;

  // This map tracks all of the navigations ongoing that are not committed
  // yet. Once a navigation is committed, it moves from the map to
  // committed_load_. Note that a PageLoadTrackers NavigationHandle is only
  // valid until commit time, when we remove it from the map.
  base::ScopedPtrMap<content::NavigationHandle*, scoped_ptr<PageLoadTracker>>
      provisional_loads_;
  scoped_ptr<PageLoadTracker> committed_load_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserver);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_WEB_CONTENTS_OBSERVER_H_
