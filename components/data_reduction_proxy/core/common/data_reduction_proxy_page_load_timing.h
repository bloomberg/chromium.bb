// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PAGE_LOAD_TIMING_H
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PAGE_LOAD_TIMING_H

#include <stdint.h>

#include "base/optional.h"
#include "base/time/time.h"

namespace data_reduction_proxy {

// The timing information that is relevant to the Pageload metrics pingback.
struct DataReductionProxyPageLoadTiming {
  DataReductionProxyPageLoadTiming(
      const base::Time& navigation_start,
      const base::Optional<base::TimeDelta>& response_start,
      const base::Optional<base::TimeDelta>& load_event_start,
      const base::Optional<base::TimeDelta>& first_image_paint,
      const base::Optional<base::TimeDelta>& first_contentful_paint,
      const base::Optional<base::TimeDelta>&
          experimental_first_meaningful_paint,
      const base::Optional<base::TimeDelta>&
          parse_blocked_on_script_load_duration,
      const base::Optional<base::TimeDelta>& parse_stop,
      int64_t network_bytes,
      int64_t original_network_bytes,
      bool app_background_occurred,
      bool opt_out_occurred);

  DataReductionProxyPageLoadTiming(
      const DataReductionProxyPageLoadTiming& other);

  // Time that the navigation for the associated page was initiated.
  const base::Time navigation_start;

  // All TimeDeltas are relative to navigation_start.

  // Time that the first byte of the response is received.
  const base::Optional<base::TimeDelta> response_start;

  // Time immediately before the load event is fired.
  const base::Optional<base::TimeDelta> load_event_start;

  // Time when the first image is painted.
  const base::Optional<base::TimeDelta> first_image_paint;
  // Time when the first contentful thing (image, text, etc.) is painted.
  const base::Optional<base::TimeDelta> first_contentful_paint;
  // (Experimental) Time when the page's primary content is painted.
  const base::Optional<base::TimeDelta> experimental_first_meaningful_paint;
  // Time that parsing was blocked by loading script.
  const base::Optional<base::TimeDelta> parse_blocked_on_script_load_duration;
  // Time when parsing completed.
  const base::Optional<base::TimeDelta> parse_stop;
  // The number of bytes served over the network, not including headers.
  const int64_t network_bytes;
  // The number of bytes that would have been served over the network if the
  // user were not using data reduction proxy, not including headers.
  const int64_t original_network_bytes;
  // True when android app background occurred during the page load lifetime.
  const bool app_background_occurred;
  // True when the user clicks "Show Original" on the Previews infobar.
  const bool opt_out_occurred;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PAGE_LOAD_TIMING_H
