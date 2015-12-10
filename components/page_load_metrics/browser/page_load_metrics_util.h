// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UTIL_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UTIL_H_

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

#define PAGE_LOAD_HISTOGRAM(name, sample)                           \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                          \
                             base::TimeDelta::FromMilliseconds(10), \
                             base::TimeDelta::FromMinutes(10), 100)

namespace page_load_metrics {

struct PageLoadExtraInfo;
struct PageLoadTiming;

// Get the time of the first 'contentful' paint. A contentful paint is a paint
// that includes content of some kind (for example, text or image content).
// Painting of a background color is not considered 'contentful'.
base::TimeDelta GetFirstContentfulPaint(const PageLoadTiming& timing);

// Returns false for events for which we have no timing information, and events
// that happened on a page that had been in the background. When a page is
// backgrounded, some events (e.g. paint) are delayed. Since these data points
// can skew the mean, they should not be mixed with timing events that occurred
// in the foreground.
bool EventOccurredInForeground(const base::TimeDelta& event,
                               const PageLoadExtraInfo& info);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UTIL_H_
