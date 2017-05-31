// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_OFFLINE_METRICS_COLLECTOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_OFFLINE_METRICS_COLLECTOR_H_

namespace offline_pages {

// Observes various events and collects the data in order to classify
// a day as 'offline', 'online' etc. Keeps the accumulated counters of each day
// type until it is a good moment to report it (most often on a connected day).
// The actual reporting is done by UMA.
class OfflineMetricsCollector {
 public:
  virtual ~OfflineMetricsCollector() = default;

  // Chrome started up or was (as on Android) brought from background to
  // foreground. A day when this happened is a day the browser was used.
  virtual void OnAppStartupOrResume() = 0;

  // Successful online navigation committed. A day when this happens is counted
  // as 'connected' day.
  virtual void OnSuccessfulNavigationOnline() = 0;

  // Successful navigation to an offline page happened. A day when it happens is
  // at least a 'offline_content' day
  virtual void OnSuccessfulNavigationOffline() = 0;

  // Uses UMA to report the accumulated classification for the days past.
  virtual void ReportAccumulatedStats() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_OFFLINE_METRICS_COLLECTOR_H_
