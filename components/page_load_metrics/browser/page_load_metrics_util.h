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

struct PageLoadTiming;

// Get the time of the first 'contentful' paint. A contentful paint is a paint
// that includes content of some kind (for example, text or image content).
// Painting of a background color is not considered 'contentful'.
base::TimeDelta GetFirstContentfulPaint(const PageLoadTiming& timing);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_UTIL_H_
