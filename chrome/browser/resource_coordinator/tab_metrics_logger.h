// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_METRICS_LOGGER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_METRICS_LOGGER_H_

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/page_transition_types.h"

namespace content {
class WebContents;
}

// Logs metrics for a tab and its WebContents when requested.
// Must be used on the UI thread.
class TabMetricsLogger {
 public:
  // The state of the page loaded in a tab's main frame, starting since the last
  // navigation.
  struct PageMetrics {
    // Number of key events.
    int key_event_count = 0;
    // Number of mouse events.
    int mouse_event_count = 0;
    // Number of touch events.
    int touch_event_count = 0;
  };

  // The state of a tab.
  struct TabMetrics {
    content::WebContents* web_contents = nullptr;

    // Source of the last committed navigation.
    ui::PageTransition page_transition = ui::PAGE_TRANSITION_FIRST;

    // Per-page metrics of the state of the WebContents. Tracked since the
    // tab's last top-level navigation.
    PageMetrics page_metrics = {};
  };

  TabMetricsLogger();
  ~TabMetricsLogger();

  // Logs metrics for the tab with the given main frame WebContents. Does
  // nothing if |ukm_source_id| is zero.
  void LogBackgroundTab(ukm::SourceId ukm_source_id,
                        const TabMetrics& tab_metrics);

  // Returns the ContentType that matches |mime_type|.
  static metrics::TabMetricsEvent::ContentType GetContentTypeFromMimeType(
      const std::string& mime_type);

  // Returns the ProtocolHandlerScheme enumerator matching the string.
  // The enumerator value is used in the UKM entry, since UKM entries can't
  // store strings.
  static metrics::TabMetricsEvent::ProtocolHandlerScheme
  GetSchemeValueFromString(const std::string& scheme);

  // Returns the site engagement score for the WebContents, rounded down to 10s
  // to limit granularity. Returns -1 if site engagement service is disabled.
  static int GetSiteEngagementScore(const content::WebContents* web_contents);

 private:
  // A counter to be incremented and logged with each UKM entry, used to
  // indicate the order that events within the same report were logged.
  int sequence_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TabMetricsLogger);
};

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_METRICS_LOGGER_H_
