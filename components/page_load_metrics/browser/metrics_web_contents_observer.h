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

// These constants are for keeping the tests in sync.
const char kHistogramNameFirstLayout[] =
    "PageLoad.Timing2.NavigationToFirstLayout";
const char kHistogramNameFirstTextPaint[] =
    "PageLoad.Timing2.NavigationToFirstTextPaint";
const char kHistogramNameDomContent[] =
    "PageLoad.Timing2.NavigationToDOMContentLoadedEventFired";
const char kHistogramNameLoad[] = "PageLoad.Timing2.NavigationToLoadEventFired";
const char kBGHistogramNameFirstLayout[] =
    "PageLoad.Timing2.NavigationToFirstLayout.Background";
const char kBGHistogramNameFirstTextPaint[] =
    "PageLoad.Timing2.NavigationToFirstTextPaint.Background";
const char kBGHistogramNameDomContent[] =
    "PageLoad.Timing2.NavigationToDOMContentLoadedEventFired.Background";
const char kBGHistogramNameLoad[] =
    "PageLoad.Timing2.NavigationToLoadEventFired.Background";

const char kProvisionalEvents[] = "PageLoad.Events.Provisional";
const char kCommittedEvents[] = "PageLoad.Events.Committed";
const char kBGProvisionalEvents[] = "PageLoad.Events.Provisional.Background";
const char kBGCommittedEvents[] = "PageLoad.Events.Committed.Background";

const char kErrorEvents[] = "PageLoad.Events.InternalError";


// NOTE: Some of these histograms are separated into a separate histogram
// specified by the ".Background" suffix. For these events, we put them into the
// background histogram if the web contents was ever in the background from
// navigation start to the event in question.

// ProvisionalLoadEvents count all main frame navigations before they commit.
// The events in this enum are all disjoint, and summing them yields the total
// number of main frame provisional loads.
//
// If you add elements from this enum, make sure you update the enum
// value in histograms.xml. Only add elements to the end to prevent
// inconsistencies between versions.
enum ProvisionalLoadEvent {
  // This case occurs when the NavigationHandle finishes and reports
  // !HasCommitted(), but reports no net::Error. This should not occur
  // pre-PlzNavigate, but afterwards it should represent the navigation stopped
  // by the user before it was ready to commit.
  PROVISIONAL_LOAD_STOPPED,

  // An aborted provisional load has error net::ERR_ABORTED. Note that this can
  // come from some non user-initiated errors, such as downloads, or 204
  // responses. See crbug.com/542369.
  PROVISIONAL_LOAD_ERR_ABORTED,

  // This event captures all the other ways a provisional load can fail.
  PROVISIONAL_LOAD_ERR_FAILED_NON_ABORT,

  // Counts the number of successful commits.
  PROVISIONAL_LOAD_COMMITTED,

  // Add values before this final count.
  PROVISIONAL_LOAD_LAST_ENTRY
};

// CommittedLoadEvents are events that occur on committed loads that we track.
// Note that we capture events only for committed loads that:
//  - Are http/https.
//  - Not same-page navigations.
//  - Are not navigations to an error page.
// We only know these things about a navigation post-commit.
//
// If you add elements to this enum, make sure you update the enum
// value in histograms.xml. Only add elements to the end to prevent
// inconsistencies between versions.
enum CommittedLoadEvent {
  // When a load that eventually commits started. Note we can't log this until
  // commit time, but it represents when the actual page load started. Thus, we
  // only separate this into .Background when a page load starts backgrounded.
  COMMITTED_LOAD_STARTED,

  // These two events are disjoint. Sum them to find the total number of
  // committed loads that we end up tracking.
  COMMITTED_LOAD_FAILED_BEFORE_FIRST_LAYOUT,
  COMMITTED_LOAD_SUCCESSFUL_FIRST_LAYOUT,

  // TODO(csharrison) once first paint metrics are in place, add new events.

  // Add values before this final count.
  COMMITTED_LOAD_LAST_ENTRY
};

// These errors are internal to the page_load_metrics subsystem and do not
// reflect actual errors that occur during a page load.
//
// If you add elements to this enum, make sure you update the enum
// value in histograms.xml. Only add elements to the end to prevent
// inconsistencies between versions.
enum InternalErrorLoadEvent {
  // A timing IPC was sent from the renderer that did not line up with previous
  // data we've received (i.e. navigation start is different or the timing
  // struct is somehow invalid). This error can only occur once the IPC is
  // vetted in other ways (see other errors).
  ERR_BAD_TIMING_IPC,

  // The following IPCs are not mutually exclusive.
  //
  // We received an IPC when we weren't tracking a committed load. This will
  // often happen if we get an IPC from a bad URL scheme (that is, the renderer
  // sent us an IPC from a navigation we don't care about).
  ERR_IPC_WITH_NO_COMMITTED_LOAD,

  // Received a notification from a frame that has been navigated away from.
  ERR_IPC_FROM_WRONG_FRAME,

  // We received an IPC even through the last committed url from the browser
  // was not http/s. This can happen with the renderer sending IPCs for the
  // new tab page. This will often come paired with
  // ERR_IPC_WITH_NO_COMMITTED_LOAD.
  ERR_IPC_FROM_BAD_URL_SCHEME,

  // If we track a navigation, but the renderer sends us no IPCs. This could
  // occur if the browser filters loads less aggressively than the renderer.
  ERR_NO_IPCS_RECEIVED,

  // Add values before this final count.
  ERR_LAST_ENTRY
};

class PageLoadTracker {
 public:
  explicit PageLoadTracker(bool in_foreground);
  ~PageLoadTracker();
  void Commit();
  void WebContentsHidden();
  void WebContentsShown();

  // Returns true if the timing was successfully updated.
  bool UpdateTiming(const PageLoadTiming& timing);
  void RecordProvisionalEvent(ProvisionalLoadEvent event);
  void RecordCommittedEvent(CommittedLoadEvent event, bool backgrounded);
  bool HasBackgrounded();

 private:
  void RecordTimingHistograms();

  bool has_commit_;

  // We record separate metrics for events that occur after a background,
  // because metrics like layout/paint are delayed artificially
  // when they occur in the background.
  base::TimeTicks background_time_;
  base::TimeTicks foreground_time_;
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
