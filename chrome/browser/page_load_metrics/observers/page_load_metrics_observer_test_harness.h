// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/test/web_contents_tester.h"

namespace page_load_metrics {

// This class can be used to drive tests of PageLoadMetricsObservers. To hook up
// an observer, override RegisterObservers and call tracker->AddObserver. This
// will attach the observer to all main frame navigations.
class PageLoadMetricsObserverTestHarness
    : public ChromeRenderViewHostTestHarness {
 public:
  PageLoadMetricsObserverTestHarness();
  ~PageLoadMetricsObserverTestHarness() override;

  // Helper that fills in any timing fields that MWCO requires but that are
  // currently missing.
  static void PopulateRequiredTimingFields(PageLoadTiming* inout_timing);

  void SetUp() override;

  virtual void RegisterObservers(PageLoadTracker* tracker) {}

  // Simulates starting a navigation to the given gurl, without committing the
  // navigation.
  void StartNavigation(const GURL& gurl);

  // Call this to simulate sending a PageLoadTiming IPC from the render process
  // to the browser process. These will update the timing information for the
  // most recently committed navigation.
  void SimulateTimingUpdate(const PageLoadTiming& timing);
  void SimulateTimingAndMetadataUpdate(const PageLoadTiming& timing,
                                       const PageLoadMetadata& metadata);

  const base::HistogramTester& histogram_tester() const;

 private:
  base::HistogramTester histogram_tester_;
  MetricsWebContentsObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsObserverTestHarness);
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TEST_HARNESS_H_
