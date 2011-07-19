// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRIC_EVENT_DURATION_DETAILS_H_
#define CHROME_BROWSER_METRICS_METRIC_EVENT_DURATION_DETAILS_H_
#pragma once

#include <string>

// Used when sending a notification about an event that occurred that we want
// to time.
struct MetricEventDurationDetails {
  MetricEventDurationDetails(const std::string& e, int d)
      : event_name(e), duration_ms(d) {}

  std::string event_name;
  int duration_ms;
};

#endif  // CHROME_BROWSER_METRICS_METRIC_EVENT_DURATION_DETAILS_H_
