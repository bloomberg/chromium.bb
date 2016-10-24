// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_
#define CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Measures start up performance of the first active web contents.
// This class is declared on all platforms, but only defined on non-Android
// platforms. Android code should not call any non-trivial methods on this
// class.
class FirstWebContentsProfiler : public content::WebContentsObserver {
 public:
  // Creates a profiler for the active web contents. If there are multiple
  // browsers, the first one is chosen. The resulting FirstWebContentsProfiler
  // owns itself.
  static void Start();

 private:
  // Reasons for which profiling is deemed complete. Logged in UMA (do not re-
  // order or re-assign).
  enum FinishReason {
    // All metrics were successfully gathered.
    DONE = 0,
    // Abandon if blocking UI was shown during startup.
    ABANDON_BLOCKING_UI = 1,
    // Abandon if the content is hidden (lowers scheduling priority).
    ABANDON_CONTENT_HIDDEN = 2,
    // Abandon if the content is destroyed.
    ABANDON_CONTENT_DESTROYED = 3,
    // Abandon if the WebContents navigates away from its initial page.
    ABANDON_NEW_NAVIGATION = 4,
    // Abandon if the WebContents fails to load (e.g. network error, etc.).
    ABANDON_NAVIGATION_ERROR = 5,
    ENUM_MAX
  };
  explicit FirstWebContentsProfiler(content::WebContents* web_contents);
  ~FirstWebContentsProfiler() override = default;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WasHidden() override;
  void WebContentsDestroyed() override;

  // Whether this instance has finished collecting first-paint and main-frame-
  // load metrics (navigation metrics are recorded on a best effort but don't
  // prevent the FirstWebContentsProfiler from calling it).
  bool IsFinishedCollectingMetrics();

  // Logs |finish_reason| to UMA and deletes this FirstWebContentsProfiler.
  void FinishedCollectingMetrics(FinishReason finish_reason);

  // Whether an attempt was made to collect the "NonEmptyPaint" metric.
  bool collected_paint_metric_;

  // Whether an attempt was made to collect the "MainFrameLoad" metric.
  bool collected_load_metric_;

  // Whether an attempt was made to collect the "MainNavigationStart" metric.
  bool collected_main_navigation_start_metric_;

  // Whether an attempt was made to collect the "MainNavigationFinished" metric.
  bool collected_main_navigation_finished_metric_;

  DISALLOW_COPY_AND_ASSIGN(FirstWebContentsProfiler);
};

#endif  // CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_
