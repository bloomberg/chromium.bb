// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_METRICS_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_CHROME_BROWSER_METRICS_SERVICE_OBSERVER_H_

#include "components/metrics/metrics_service_observer.h"

// ChromeBrowserMetricsServiceObserver receives notifications when the metrics
// service creates a new metrics log.
class ChromeBrowserMetricsServiceObserver : public MetricsServiceObserver {
 public:
  ChromeBrowserMetricsServiceObserver();
  virtual ~ChromeBrowserMetricsServiceObserver();

  // MetricsServiceObserver:
  virtual void OnDidCreateMetricsLog() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMetricsServiceObserver);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_METRICS_SERVICE_OBSERVER_H_
