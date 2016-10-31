// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CSS_SCANNING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CSS_SCANNING_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

class CssScanningMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  CssScanningMetricsObserver();
  ~CssScanningMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstMeaningfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssScanningMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CSS_SCANNING_PAGE_LOAD_METRICS_OBSERVER_H_
