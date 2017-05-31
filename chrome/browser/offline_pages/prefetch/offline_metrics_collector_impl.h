// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"

namespace offline_pages {

class OfflineMetricsCollectorImpl : public OfflineMetricsCollector {
 public:
  OfflineMetricsCollectorImpl();
  ~OfflineMetricsCollectorImpl() override;

  // OfflineMetricsCollector implementation.
  void OnAppStartupOrResume() override;
  void OnSuccessfulNavigationOnline() override;
  void OnSuccessfulNavigationOffline() override;
  void ReportAccumulatedStats() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OfflineMetricsCollectorImpl);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_
