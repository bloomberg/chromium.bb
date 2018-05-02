// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_WINDOW_FEATURES_H_

#include <string>

#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "components/sessions/core/session_id.h"

// Window features used for logging a WindowMetrics event to UKM.
struct WindowFeatures {
  WindowFeatures(SessionID window_id, metrics::WindowMetricsEvent::Type type);
  WindowFeatures(const WindowFeatures& other);
  ~WindowFeatures();

  bool operator==(const WindowFeatures& other) const;
  bool operator!=(const WindowFeatures& other) const;

  // ID for the window, unique within the Chrome session.
  const SessionID window_id;
  const metrics::WindowMetricsEvent::Type type;
  metrics::WindowMetricsEvent::ShowState show_state =
      metrics::WindowMetricsEvent::SHOW_STATE_UNKNOWN;

  // True if this is the active (frontmost) window.
  bool is_active = false;

  // Number of tabs in the tab strip.
  int tab_count = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_WINDOW_FEATURES_H_
