// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_H_
#define CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_H_

#include "base/macros.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class WebContents;
}

// Logs metrics for a tab and its WebContents when requested.
// Must be used on the UI thread.
class TabMetricsLogger {
 public:
  virtual ~TabMetricsLogger() = default;

  // Logs metrics for the tab with the given main frame WebContents. Does
  // nothing if |ukm_source_id| is zero.
  virtual void LogBackgroundTab(ukm::SourceId ukm_source_id,
                                content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_H_
