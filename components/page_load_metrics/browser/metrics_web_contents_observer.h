// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_WEB_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace IPC {
class Message;
}  // namespace IPC

namespace page_load_metrics {

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
  void DidCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  explicit MetricsWebContentsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<MetricsWebContentsObserver>;
  friend class MetricsWebContentsObserverTest;

  void OnTimingUpdated(content::RenderFrameHost*, const PageLoadTiming& timing);
  void RecordTimingHistograms();

  bool IsRelevantNavigation(content::NavigationHandle* navigation_handle);

  // Will be null before any navigations have committed. When a navigation
  // commits we will initialize this as empty.
  scoped_ptr<PageLoadTiming> current_timing_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserver);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_WEB_CONTENTS_OBSERVER_H_
