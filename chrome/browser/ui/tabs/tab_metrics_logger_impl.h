// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_metrics_logger.h"

// Logs a TabManager.TabMetrics UKM for a tab when requested. Includes
// information relevant to the tab and its WebContents.
// Must be used on the UI thread.
// TODO(michaelpg): Unit test for UKMs.
class TabMetricsLoggerImpl : public TabMetricsLogger {
 public:
  TabMetricsLoggerImpl();
  ~TabMetricsLoggerImpl() override;

  // TabMetricsLogger:
  void LogBackgroundTab(ukm::SourceId ukm_source_id,
                        content::WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabMetricsLoggerImpl);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_IMPL_H_
