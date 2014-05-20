// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_OBSERVER_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_OBSERVER_H_

#include "base/macros.h"

// MetricsServiceObserver receives notifications from MetricsService.
// An observer must be added, removed, and notified on the same thread.
class MetricsServiceObserver {
 public:
  // Called when a new MetricsLog is created.
  virtual void OnDidCreateMetricsLog() = 0;

 protected:
  MetricsServiceObserver();
  virtual ~MetricsServiceObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsServiceObserver);
};

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_OBSERVER_H_
