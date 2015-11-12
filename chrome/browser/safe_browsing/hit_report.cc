// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/hit_report.h"

#include "chrome/browser/browser_process.h"
#include "components/metrics/metrics_service.h"

namespace safe_browsing {

HitReport::HitReport() {}

HitReport::~HitReport() {}

bool IsMetricsReportingActive() {
  const metrics::MetricsService* metrics = g_browser_process->metrics_service();
  return metrics && metrics->reporting_active();
}

}  // namespace safe_browsing
