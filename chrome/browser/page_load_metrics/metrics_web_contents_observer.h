// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_METRICS_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_METRICS_WEB_CONTENTS_OBSERVER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/resource_type.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace IPC {
class Message;
}  // namespace IPC

namespace page_load_metrics {

class PageLoadMetricsEmbedderInterface;
class PageLoadTracker;

// MetricsWebContentsObserver tracks page loads and loading metrics
// related data based on IPC messages received from a
// MetricsRenderFrameObserver.
class MetricsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetricsWebContentsObserver>,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  // Note that the returned metrics is owned by the web contents.
  static MetricsWebContentsObserver* CreateForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);
  MetricsWebContentsObserver(
      content::WebContents* web_contents,
      std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);
  ~MetricsWebContentsObserver() override;

  // content::WebContentsObserver implementation:
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationStopped() override;
  void OnInputEvent(const blink::WebInputEvent& event) override;
  void WasShown() override;
  void WasHidden() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  // These methods are forwarded from the MetricsNavigationThrottle.
  void WillStartNavigationRequest(content::NavigationHandle* navigation_handle);
  void WillProcessNavigationResponse(
      content::NavigationHandle* navigation_handle);

  // A resource request completed on the IO thread. This method is invoked on
  // the UI thread.
  void OnRequestComplete(const content::GlobalRequestID& request_id,
                         content::ResourceType resource_type,
                         bool was_cached,
                         bool used_data_reduction_proxy,
                         int64_t raw_body_bytes,
                         int64_t original_content_length,
                         base::TimeTicks creation_time);

  // Flush any buffered metrics, as part of the metrics subsystem persisting
  // metrics as the application goes into the background. The application may be
  // killed at any time after this method is invoked without further
  // notification.
  void FlushMetricsOnAppEnterBackground();

  // This getter function is required for testing.
  const PageLoadExtraInfo GetPageLoadExtraInfoForCommittedLoad();

 private:
  friend class content::WebContentsUserData<MetricsWebContentsObserver>;

  void HandleFailedNavigationForTrackedLoad(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<PageLoadTracker> tracker);

  void HandleCommittedNavigationForTrackedLoad(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<PageLoadTracker> tracker);

  // Return a PageLoadTracker (either provisional or committed) that matches the
  // given request attributes, or nullptr if there are no matching
  // PageLoadTrackers.
  PageLoadTracker* GetTrackerOrNullForRequest(
      const content::GlobalRequestID& request_id,
      content::ResourceType resource_type,
      base::TimeTicks creation_time);

  // Notify all loads, provisional and committed, that we performed an action
  // that might abort them.
  void NotifyPageEndAllLoads(PageEndReason page_end_reason,
                             UserInitiatedInfo user_initiated_info);
  void NotifyPageEndAllLoadsWithTimestamp(PageEndReason page_end_reason,
                                          UserInitiatedInfo user_initiated_info,
                                          base::TimeTicks timestamp,
                                          bool is_certainly_browser_timestamp);

  // Register / Unregister input event callback to given RenderViewHost
  void RegisterInputEventObserver(content::RenderViewHost* host);
  void UnregisterInputEventObserver(content::RenderViewHost* host);

  // Notify aborted provisional loads that a new navigation occurred. This is
  // used for more consistent attribution tracking for aborted provisional
  // loads. This method returns the provisional load that was likely aborted
  // by this navigation, to help instantiate the new PageLoadTracker.
  std::unique_ptr<PageLoadTracker> NotifyAbortedProvisionalLoadsNewNavigation(
      content::NavigationHandle* new_navigation,
      UserInitiatedInfo user_initiated_info);

  void OnTimingUpdated(content::RenderFrameHost*,
                       const PageLoadTiming& timing,
                       const PageLoadMetadata& metadata);

  bool ShouldTrackNavigation(
      content::NavigationHandle* navigation_handle) const;

  // True if the web contents is currently in the foreground.
  bool in_foreground_;

  // The PageLoadTrackers must be deleted before the |embedder_interface_|,
  // because they hold a pointer to the |embedder_interface_|.
  std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface_;

  // This map tracks all of the navigations ongoing that are not committed
  // yet. Once a navigation is committed, it moves from the map to
  // committed_load_. Note that a PageLoadTrackers NavigationHandle is only
  // valid until commit time, when we remove it from the map.
  std::map<content::NavigationHandle*, std::unique_ptr<PageLoadTracker>>
      provisional_loads_;

  // Tracks aborted provisional loads for a little bit longer than usual (one
  // more navigation commit at the max), in order to better understand how the
  // navigation failed. This is because most provisional loads are destroyed
  // and vanish before we get signal about what caused the abort (new
  // navigation, stop button, etc.).
  std::vector<std::unique_ptr<PageLoadTracker>> aborted_provisional_loads_;

  std::unique_ptr<PageLoadTracker> committed_load_;

  // Has the MWCO observed at least one navigation?
  bool has_navigated_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserver);
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_METRICS_WEB_CONTENTS_OBSERVER_H_
