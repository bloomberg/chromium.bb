// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_
#define CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
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
  class Delegate {
   public:
    // Called by the FirstWebContentsProfiler when it is finished collecting
    // metrics. The delegate should take this opportunity to destroy the
    // FirstWebContentsProfiler.
    virtual void ProfilerFinishedCollectingMetrics() = 0;
  };

  // Creates a profiler for the active web contents. If there are multiple
  // browsers, the first one is chosen. If there are no browsers, returns
  // nullptr.
  static scoped_ptr<FirstWebContentsProfiler> CreateProfilerForFirstWebContents(
      Delegate* delegate);

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
    ABANDON_NAVIGATION = 4,
    ENUM_MAX
  };

  FirstWebContentsProfiler(content::WebContents* web_contents,
                           Delegate* delegate);

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WasHidden() override;
  void WebContentsDestroyed() override;

  // Whether this instance has finished collecting all of its metrics.
  bool IsFinishedCollectingMetrics();

  // Informs the delegate that this instance has finished collecting all of its
  // metrics. Logs |finish_reason| to UMA.
  void FinishedCollectingMetrics(FinishReason finish_reason);

  // Initialize histograms for unresponsiveness metrics.
  void InitHistograms();

  // Becomes true on the first invocation of NavigationEntryCommitted() for the
  // main frame. Any follow-up navigation to a different page will result in
  // aborting first WebContents profiling.
  bool initial_entry_committed_;

  // Whether an attempt was made to collect the "NonEmptyPaint" metric.
  bool collected_paint_metric_;

  // Whether an attempt was made to collect the "MainFrameLoad" metric.
  bool collected_load_metric_;

  // |delegate_| owns |this|.
  Delegate* delegate_;

  // Histogram that keeps track of response times for the watched thread.
  base::HistogramBase* responsiveness_histogram_;

  // Histogram that keeps track of response times for the watched thread.
  base::HistogramBase* responsiveness_1sec_histogram_;

  // Histogram that keeps track of response times for the watched thread.
  base::HistogramBase* responsiveness_10sec_histogram_;

  // Histogram that keeps track of response times for the watched thread.
  base::HistogramBase* unresponsiveness_histogram_;

  // Histogram that keeps track of response times for the watched thread.
  base::HistogramBase* unresponsiveness_1sec_histogram_;

  // Histogram that keeps track of response times for the watched thread.
  base::HistogramBase* unresponsiveness_10sec_histogram_;

  DISALLOW_COPY_AND_ASSIGN(FirstWebContentsProfiler);
};

#endif  // CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_
