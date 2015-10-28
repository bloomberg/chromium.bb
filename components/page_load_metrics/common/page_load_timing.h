// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_TIMING_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_TIMING_H_

#include "base/time/time.h"

namespace page_load_metrics {

// PageLoadTiming contains timing metrics associated with a page load. Many of
// the metrics here are based on the Navigation Timing spec:
// http://www.w3.org/TR/navigation-timing/.
struct PageLoadTiming {
 public:
  PageLoadTiming();
  ~PageLoadTiming();

  bool operator==(const PageLoadTiming& other) const;

  bool IsEmpty() const;

  // Time that the navigation for the associated page was initiated.
  base::Time navigation_start;

  // All TimeDeltas are relative to navigation_start

  // Time that the first byte of the response is received.
  base::TimeDelta response_start;

  // Time immediately before the DOMContentLoaded event is fired.
  base::TimeDelta dom_content_loaded_event_start;

  // Time immediately before the load event is fired.
  base::TimeDelta load_event_start;

  // Time when the first layout is completed.
  base::TimeDelta first_layout;

  // Time when the first paint is performed.
  base::TimeDelta first_paint;
  // Time when the first non-blank text is painted.
  base::TimeDelta first_text_paint;
  // Time when the first image is painted.
  base::TimeDelta first_image_paint;

  // If you add additional members, also be sure to update operator==,
  // page_load_metrics_messages.h, and IsEmpty().
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_TIMING_H_
