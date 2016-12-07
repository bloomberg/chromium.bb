// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_LOAD_METRICS_PAGE_LOAD_TIMING_H_
#define CHROME_COMMON_PAGE_LOAD_METRICS_PAGE_LOAD_TIMING_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace page_load_metrics {

struct StyleSheetTiming {
  StyleSheetTiming();
  StyleSheetTiming(const StyleSheetTiming& other);
  ~StyleSheetTiming();

  bool operator==(const StyleSheetTiming& other) const;
  bool operator!=(const StyleSheetTiming& other) const {
    return !(*this == other);
  }

  bool IsEmpty() const;

  // Total time spent parsing author style sheets, before the first contentful
  // paint.
  base::Optional<base::TimeDelta> author_style_sheet_parse_duration_before_fcp;

  // Time spent in Document::updateStyle before FCP.
  base::Optional<base::TimeDelta> update_style_duration_before_fcp;
};

// PageLoadTiming contains timing metrics associated with a page load. Many of
// the metrics here are based on the Navigation Timing spec:
// http://www.w3.org/TR/navigation-timing/.
struct PageLoadTiming {
 public:
  PageLoadTiming();
  PageLoadTiming(const PageLoadTiming& other);
  ~PageLoadTiming();

  bool operator==(const PageLoadTiming& other) const;
  bool operator!=(const PageLoadTiming& other) const {
    return !(*this == other);
  }

  bool IsEmpty() const;

  // Time that the navigation for the associated page was initiated. Note that
  // this field is only used for internal tracking purposes and should not be
  // used by PageLoadMetricsObservers. This field will likely be removed in the
  // future.
  base::Time navigation_start;

  // TimeDeltas relative to navigation_start:

  // Time that the first byte of the response is received.
  base::Optional<base::TimeDelta> response_start;

  // Time immediately before the DOMContentLoaded event is fired.
  base::Optional<base::TimeDelta> dom_content_loaded_event_start;

  // Time immediately before the load event is fired.
  base::Optional<base::TimeDelta> load_event_start;

  // Time when the first layout is completed.
  base::Optional<base::TimeDelta> first_layout;

  // Time when the first paint is performed.
  base::Optional<base::TimeDelta> first_paint;
  // Time when the first non-blank text is painted.
  base::Optional<base::TimeDelta> first_text_paint;
  // Time when the first image is painted.
  base::Optional<base::TimeDelta> first_image_paint;
  // Time when the first contentful thing (image, text, etc.) is painted.
  base::Optional<base::TimeDelta> first_contentful_paint;
  // (Experimental) Time when the page's primary content is painted.
  base::Optional<base::TimeDelta> first_meaningful_paint;


  // TimeDeltas below represent durations of time during the page load:

  // Time that the document's parser started and stopped parsing main resource
  // content.
  base::Optional<base::TimeDelta> parse_start;
  base::Optional<base::TimeDelta> parse_stop;

  // Sum of times when the parser is blocked waiting on the load of a script.
  // This duration takes place between parser_start and parser_stop, and thus
  // must be less than or equal to parser_stop - parser_start. Note that this
  // value may be updated multiple times during the period between parse_start
  // and parse_stop.
  base::Optional<base::TimeDelta> parse_blocked_on_script_load_duration;

  // Sum of times when the parser is blocked waiting on the load of a script
  // that was inserted from document.write. This duration must be less than or
  // equal to parse_blocked_on_script_load_duration. Note that this value may be
  // updated multiple times during the period between parse_start and
  // parse_stop. Note that some uncommon cases where scripts are loaded via
  // document.write are not currently covered by this field. See crbug/600711
  // for details.
  base::Optional<base::TimeDelta>
      parse_blocked_on_script_load_from_document_write_duration;

  // Sum of times when the parser is executing a script.  This duration takes
  // place between parser_start and parser_stop, and thus must be less than or
  // equal to parser_stop - parser_start. Note that this value may be updated
  // multiple times during the period between parse_start and parse_stop.
  base::Optional<base::TimeDelta> parse_blocked_on_script_execution_duration;

  // Sum of times when the parser is executing a script that was inserted from
  // document.write. This duration must be less than or equal to
  // parse_blocked_on_script_load_duration. Note that this value may be updated
  // multiple times during the period between parse_start and parse_stop. Note
  // that some uncommon cases where scripts are loaded via document.write are
  // not currently covered by this field. See crbug/600711 for details.
  base::Optional<base::TimeDelta>
      parse_blocked_on_script_execution_from_document_write_duration;

  StyleSheetTiming style_sheet_timing;

  // If you add additional members, also be sure to update operator==,
  // page_load_metrics_messages.h, and IsEmpty().
};

struct PageLoadMetadata {
  PageLoadMetadata();
  bool operator==(const PageLoadMetadata& other) const;
  // These are packed blink::WebLoadingBehaviorFlag enums.
  int behavior_flags = blink::WebLoadingBehaviorNone;
};

}  // namespace page_load_metrics

#endif  // CHROME_COMMON_PAGE_LOAD_METRICS_PAGE_LOAD_TIMING_H_
