// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/page_load_metrics/page_load_timing.h"

namespace page_load_metrics {

mojom::PageLoadTimingPtr CreatePageLoadTiming() {
  return mojom::PageLoadTiming::New(
      base::Time(), base::Optional<base::TimeDelta>(),
      mojom::DocumentTiming::New(), mojom::InteractiveTiming::New(),
      mojom::PaintTiming::New(), mojom::ParseTiming::New(),
      mojom::StyleSheetTiming::New());
}

bool IsEmpty(const page_load_metrics::mojom::DocumentTiming& timing) {
  return !timing.dom_content_loaded_event_start && !timing.load_event_start &&
         !timing.first_layout;
}

bool IsEmpty(const page_load_metrics::mojom::InteractiveTiming& timing) {
  return !timing.interactive && !timing.interactive_detection;
}

bool IsEmpty(const page_load_metrics::mojom::PaintTiming& timing) {
  return !timing.first_paint && !timing.first_text_paint &&
         !timing.first_image_paint && !timing.first_contentful_paint &&
         !timing.first_meaningful_paint;
}

bool IsEmpty(const page_load_metrics::mojom::ParseTiming& timing) {
  return !timing.parse_start && !timing.parse_stop &&
         !timing.parse_blocked_on_script_load_duration &&
         !timing.parse_blocked_on_script_load_from_document_write_duration &&
         !timing.parse_blocked_on_script_execution_duration &&
         !timing.parse_blocked_on_script_execution_from_document_write_duration;
}

bool IsEmpty(const page_load_metrics::mojom::StyleSheetTiming& timing) {
  return !timing.author_style_sheet_parse_duration_before_fcp &&
         !timing.update_style_duration_before_fcp;
}

bool IsEmpty(const page_load_metrics::mojom::PageLoadTiming& timing) {
  return timing.navigation_start.is_null() && !timing.response_start &&
         (!timing.document_timing ||
          page_load_metrics::IsEmpty(*timing.document_timing)) &&
         (!timing.interactive_timing ||
          page_load_metrics::IsEmpty(*timing.interactive_timing)) &&
         (!timing.paint_timing ||
          page_load_metrics::IsEmpty(*timing.paint_timing)) &&
         (!timing.parse_timing ||
          page_load_metrics::IsEmpty(*timing.parse_timing)) &&
         (!timing.style_sheet_timing ||
          page_load_metrics::IsEmpty(*timing.style_sheet_timing));
}

void InitPageLoadTimingForTest(mojom::PageLoadTiming* timing) {
  timing->document_timing = mojom::DocumentTiming::New();
  timing->interactive_timing = mojom::InteractiveTiming::New();
  timing->paint_timing = mojom::PaintTiming::New();
  timing->parse_timing = mojom::ParseTiming::New();
  timing->style_sheet_timing = mojom::StyleSheetTiming::New();
}

}  // namespace page_load_metrics
