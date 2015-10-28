// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_timing.h"

namespace page_load_metrics {

PageLoadTiming::PageLoadTiming() {}

PageLoadTiming::~PageLoadTiming() {}

bool PageLoadTiming::operator==(const PageLoadTiming& other) const {
  return navigation_start == other.navigation_start &&
         response_start == other.response_start &&
         dom_content_loaded_event_start ==
             other.dom_content_loaded_event_start &&
         load_event_start == other.load_event_start &&
         first_layout == other.first_layout &&
         first_paint == other.first_paint &&
         first_text_paint == other.first_text_paint &&
         first_image_paint == other.first_image_paint;
}

bool PageLoadTiming::IsEmpty() const {
  return navigation_start.is_null() && response_start.is_zero() &&
         dom_content_loaded_event_start.is_zero() &&
         load_event_start.is_zero() && first_layout.is_zero() &&
         first_paint.is_zero() && first_text_paint.is_zero() &&
         first_image_paint.is_zero();
}

}  // namespace page_load_metrics
