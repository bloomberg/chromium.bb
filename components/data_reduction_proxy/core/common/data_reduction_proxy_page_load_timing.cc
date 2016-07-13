// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"

namespace data_reduction_proxy {

DataReductionProxyPageLoadTiming::DataReductionProxyPageLoadTiming(
    const base::Time& navigation_start,
    const base::Optional<base::TimeDelta>& response_start,
    const base::Optional<base::TimeDelta>& load_event_start,
    const base::Optional<base::TimeDelta>& first_image_paint,
    const base::Optional<base::TimeDelta>& first_contentful_paint)
    : navigation_start(navigation_start),
      response_start(response_start),
      load_event_start(load_event_start),
      first_image_paint(first_image_paint),
      first_contentful_paint(first_contentful_paint) {}

DataReductionProxyPageLoadTiming::DataReductionProxyPageLoadTiming(
    const DataReductionProxyPageLoadTiming& other)
    : navigation_start(other.navigation_start),
      response_start(other.response_start),
      load_event_start(other.load_event_start),
      first_image_paint(other.first_image_paint),
      first_contentful_paint(other.first_contentful_paint) {}

}  // namespace data_reduction_proxy
