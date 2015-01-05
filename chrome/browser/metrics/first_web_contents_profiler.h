// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_
#define CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_

#include "base/memory/scoped_ptr.h"
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
  FirstWebContentsProfiler(content::WebContents* web_contents,
                           Delegate* delegate);

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void WebContentsDestroyed() override;

  // Whether this instance has finished collecting all of its metrics.
  bool IsFinishedCollectingMetrics();

  // Informs the delegate that this instance has finished collecting all of its
  // metrics.
  void FinishedCollectingMetrics();

  // Whether the "NonEmptyPaint" metric has been collected. If an attempt is
  // made to collect the metric but the attempt fails, this member is set to
  // true to prevent this class from sitting around forever attempting to
  // collect the metric.
  bool collected_paint_metric_;

  // Whether the "MainFrameLoad" metric has been collected. If an attempt is
  // made to collect the metric but the attempt fails, this member is set to
  // true to prevent this class from sitting around forever attempting to
  // collect the metric.
  bool collected_load_metric_;

  // The time at which the process was created.
  base::Time process_creation_time_;

  // |delegate_| owns |this|.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(FirstWebContentsProfiler);
};

#endif  // CHROME_BROWSER_METRICS_FIRST_WEB_CONTENTS_PROFILER_H_
