// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
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

class PageLoadTracker;

namespace internal {

extern const char kErrorEvents[];
extern const char kAbortChainSizeReload[];
extern const char kAbortChainSizeForwardBack[];
extern const char kAbortChainSizeNewNavigation[];
extern const char kAbortChainSizeNoCommit[];
extern const char kAbortChainSizeSameURL[];

}  // namespace internal

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
  ERR_IPC_WITH_NO_RELEVANT_LOAD,

  // Received a notification from a frame that has been navigated away from.
  ERR_IPC_FROM_WRONG_FRAME,

  // We received an IPC even through the last committed url from the browser
  // was not http/s. This can happen with the renderer sending IPCs for the
  // new tab page. This will often come paired with
  // ERR_IPC_WITH_NO_RELEVANT_LOAD.
  ERR_IPC_FROM_BAD_URL_SCHEME,

  // If we track a navigation, but the renderer sends us no IPCs. This could
  // occur if the browser filters loads less aggressively than the renderer.
  ERR_NO_IPCS_RECEIVED,

  // Tracks frequency with which we record an abort time that occurred before
  // navigation start. This is expected to happen in some cases (see comments in
  // cc file for details). We use this error counter to understand how often it
  // happens.
  ERR_ABORT_BEFORE_NAVIGATION_START,

  // A new navigation triggers abort updates in multiple trackers in
  // |aborted_provisional_loads_|, when usually there should only be one (the
  // navigation that just aborted because of this one). If this happens, the
  // latest aborted load is used to track the chain size.
  ERR_NAVIGATION_SIGNALS_MULIPLE_ABORTED_LOADS,

  // Add values before this final count.
  ERR_LAST_ENTRY
};

// This class serves as a functional interface to various chrome// features.
// Impl version is defined in chrome/browser/page_load_metrics.
class PageLoadMetricsEmbedderInterface {
 public:
  virtual ~PageLoadMetricsEmbedderInterface() {}
  virtual bool IsPrerendering(content::WebContents* web_contents) = 0;
  virtual void RegisterObservers(PageLoadTracker* metrics) = 0;
};

// This class tracks a given page load, starting from navigation start /
// provisional load, until a new navigation commits or the navigation fails.
// MetricsWebContentsObserver manages a set of provisional PageLoadTrackers, as
// well as a committed PageLoadTracker.
class PageLoadTracker {
 public:
  // Caller must guarantee that the embedder_interface pointer outlives this
  // class. The PageLoadTracker must not hold on to
  // currently_committed_load_or_null or navigation_handle beyond the scope of
  // the constructor.
  PageLoadTracker(bool in_foreground,
                  PageLoadMetricsEmbedderInterface* embedder_interface,
                  PageLoadTracker* const currently_committed_load_or_null,
                  content::NavigationHandle* navigation_handle,
                  int aborted_chain_size,
                  int aborted_chain_size_same_url);
  ~PageLoadTracker();
  void Redirect(content::NavigationHandle* navigation_handle);
  void Commit(content::NavigationHandle* navigation_handle);
  void FailedProvisionalLoad(content::NavigationHandle* navigation_handle);
  void WebContentsHidden();
  void WebContentsShown();

  // Returns true if the timing was successfully updated.
  bool UpdateTiming(const PageLoadTiming& timing,
                    const PageLoadMetadata& metadata);

  void set_renderer_tracked(bool renderer_tracked);
  bool renderer_tracked() const { return renderer_tracked_; }

  int aborted_chain_size() const { return aborted_chain_size_; }
  int aborted_chain_size_same_url() const {
    return aborted_chain_size_same_url_;
  }

  UserAbortType abort_type() const { return abort_type_; }
  base::TimeTicks abort_time() const { return abort_time_; }

  void AddObserver(scoped_ptr<PageLoadMetricsObserver> observer);

  // If the user performs some abort-like action while we are tracking this page
  // load, notify the tracker. Note that we may not classify this as an abort if
  // we've already performed a first paint.
  void NotifyAbort(UserAbortType abort_type, base::TimeTicks timestamp);
  void UpdateAbort(UserAbortType abort_type, base::TimeTicks timestamp);

  // This method returns true if this page load has been aborted with type of
  // ABORT_OTHER, and the |abort_cause_time| is within a sufficiently close
  // delta to when it was aborted. Note that only provisional loads can be
  // aborted with ABORT_OTHER. While this heuristic is coarse, it works better
  // and is simpler than other feasible methods. See https://goo.gl/WKRG98.
  bool IsLikelyProvisionalAbort(base::TimeTicks abort_cause_time);

  bool MatchesOriginalNavigation(content::NavigationHandle* navigation_handle);

  // Only valid to call post-commit.
  const GURL& committed_url() const {
    DCHECK(!commit_time_.is_null());
    return url_;
  }

 private:
  PageLoadExtraInfo GetPageLoadMetricsInfo();

  void UpdateAbortInternal(UserAbortType abort_type,
                           base::TimeTicks timestamp);

  // If |final_navigation| is null, then this is an "unparented" abort chain,
  // and represents a sequence of provisional aborts that never ends with a
  // committed load.
  void LogAbortChainHistograms(content::NavigationHandle* final_navigation);

  // Whether the renderer should be sending timing IPCs to this page load.
  bool renderer_tracked_;

  // The navigation start in TimeTicks, not the wall time reported by Blink.
  const base::TimeTicks navigation_start_;

  // Time this page load was committed. If this page load hasn't committed,
  // |commit_time_| will be zero.
  base::TimeTicks commit_time_;

  // The URL of this page load. This is the provisional url before commit
  // (before redirects), and the committed url after commit.
  GURL url_;

  // Will be ABORT_NONE if we have not aborted this load yet. Otherwise will
  // be the first abort action the user performed.
  UserAbortType abort_type_;
  base::TimeTicks abort_time_;

  // We record separate metrics for events that occur after a background,
  // because metrics like layout/paint are delayed artificially
  // when they occur in the background.
  base::TimeTicks background_time_;
  base::TimeTicks foreground_time_;
  bool started_in_foreground_;

  PageLoadTiming timing_;
  PageLoadMetadata metadata_;

  // This is a subtle member. If a provisional load A gets aborted by
  // provisional load B, which gets aborted by C that eventually commits, then
  // there exists an abort chain of length 2, starting at A's navigation_start.
  // This is useful because it allows histograming abort chain lengths based on
  // what the last load's transition type is. i.e. holding down F-5 to spam
  // reload will produce a long chain with the RELOAD transition.
  const int aborted_chain_size_;

  // This member counts consecutive provisional aborts that share a url. It will
  // always be less than or equal to |aborted_chain_size_|.
  const int aborted_chain_size_same_url_;

  // Interface to chrome features. Must outlive the class.
  PageLoadMetricsEmbedderInterface* const embedder_interface_;

  std::vector<scoped_ptr<PageLoadMetricsObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadTracker);
};

// MetricsWebContentsObserver tracks page loads and loading metrics
// related data based on IPC messages received from a
// MetricsRenderFrameObserver.
class MetricsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetricsWebContentsObserver> {
 public:
  // Note that the returned metrics is owned by the web contents.
  static MetricsWebContentsObserver* CreateForWebContents(
      content::WebContents* web_contents,
      scoped_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);
  MetricsWebContentsObserver(
      content::WebContents* web_contents,
      scoped_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);
  ~MetricsWebContentsObserver() override;

  // content::WebContentsObserver implementation:
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationStopped() override;
  void WasShown() override;
  void WasHidden() override;
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  friend class content::WebContentsUserData<MetricsWebContentsObserver>;

  // Notify all loads, provisional and committed, that we performed an action
  // that might abort them.
  void NotifyAbortAllLoads(UserAbortType abort_type);
  void NotifyAbortAllLoadsWithTimestamp(UserAbortType abort_type,
                                        base::TimeTicks timestamp);
  // Notify aborted provisional loads that a new navigation occurred. This is
  // used for more consistent attribution tracking for aborted provisional
  // loads. This method returns the provisional load that was likely aborted by
  // this navigation, to help instantiate the new PageLoadTracker.
  scoped_ptr<PageLoadTracker> NotifyAbortedProvisionalLoadsNewNavigation(
      content::NavigationHandle* new_navigation);

  void OnTimingUpdated(content::RenderFrameHost*,
                       const PageLoadTiming& timing,
                       const PageLoadMetadata& metadata);

  // True if the web contents is currently in the foreground.
  bool in_foreground_;

  // The PageLoadTrackers must be deleted before the |embedded_interface_|,
  // because they hold a pointer to the |embedder_interface_|.
  scoped_ptr<PageLoadMetricsEmbedderInterface> embedder_interface_;

  // This map tracks all of the navigations ongoing that are not committed
  // yet. Once a navigation is committed, it moves from the map to
  // committed_load_. Note that a PageLoadTrackers NavigationHandle is only
  // valid until commit time, when we remove it from the map.
  std::map<content::NavigationHandle*, scoped_ptr<PageLoadTracker>>
      provisional_loads_;

  // Tracks aborted provisional loads for a little bit longer than usual (one
  // more navigation commit at the max), in order to better understand how the
  // navigation failed. This is because most provisional loads are destroyed and
  // vanish before we get signal about what caused the abort (new navigation,
  // stop button, etc.).
  std::vector<scoped_ptr<PageLoadTracker>> aborted_provisional_loads_;

  scoped_ptr<PageLoadTracker> committed_load_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserver);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_
