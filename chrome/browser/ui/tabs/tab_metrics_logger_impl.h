// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_IMPL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_metrics_event.pb.h"
#include "chrome/browser/ui/tabs/tab_metrics_logger.h"

// Logs a TabManager.TabMetrics UKM for a tab when requested. Includes
// information relevant to the tab and its WebContents.
// Must be used on the UI thread.
class TabMetricsLoggerImpl : public TabMetricsLogger {
 public:
  TabMetricsLoggerImpl();
  ~TabMetricsLoggerImpl() override;

  // TabMetricsLogger:
  void LogBackgroundTab(ukm::SourceId ukm_source_id,
                        const TabMetrics& tab_metrics) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TabMetricsLoggerImplTest, Schemes);
  FRIEND_TEST_ALL_PREFIXES(TabMetricsLoggerImplTest, NonWhitelistedSchemes);

  // Returns the ProtocolHandlerScheme enumerator matching the string.
  static metrics::TabMetricsEvent::ProtocolHandlerScheme
  GetSchemeValueFromString(const std::string& scheme);

  // A counter to be incremented and logged with each UKM entry, used to
  // indicate the order that events within the same report were logged.
  int sequence_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TabMetricsLoggerImpl);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_METRICS_LOGGER_IMPL_H_
