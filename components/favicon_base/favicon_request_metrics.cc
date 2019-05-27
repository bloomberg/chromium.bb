// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_request_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

void favicon::RecordFaviconAvailabilityMetric(
    favicon::FaviconRequestOrigin origin,
    favicon::FaviconAvailability availability) {
  switch (origin) {
    case favicon::FaviconRequestOrigin::HISTORY:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.HISTORY",
                                availability);
      break;
    case favicon::FaviconRequestOrigin::HISTORY_SYNCED_TABS:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.SYNCED_TABS",
                                availability);
      break;
    case favicon::FaviconRequestOrigin::RECENTLY_CLOSED_TABS:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.RECENTLY_CLOSED_TABS",
                                availability);
      break;
    case favicon::FaviconRequestOrigin::UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION("Sync.FaviconAvailability.UNKNOWN",
                                availability);
      break;
  }
}

void favicon::RecordFaviconServerGroupingMetric(
    favicon::FaviconRequestOrigin origin,
    int group_size) {
  DCHECK_GE(group_size, 0);
  switch (origin) {
    case favicon::FaviconRequestOrigin::HISTORY:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.HISTORY", group_size);
      break;
    case favicon::FaviconRequestOrigin::HISTORY_SYNCED_TABS:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.SYNCED_TABS", group_size);
      break;
    case favicon::FaviconRequestOrigin::RECENTLY_CLOSED_TABS:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.RECENTLY_CLOSED_TABS",
          group_size);
      break;
    case favicon::FaviconRequestOrigin::UNKNOWN:
      base::UmaHistogramCounts100(
          "Sync.SizeOfFaviconServerRequestGroup.UNKNOWN", group_size);
      break;
  }
}
