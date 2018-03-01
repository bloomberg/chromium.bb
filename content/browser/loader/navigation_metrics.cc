// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace content {

void RecordNavigationResourceHandlerMetrics(
    base::TimeTicks response_started,
    base::TimeTicks proceed_with_response,
    base::TimeTicks first_read_completed) {
  UMA_HISTOGRAM_TIMES(
      "Navigation.ResourceHandler."
      "ResponseStartedUntilProceedWithResponse",
      proceed_with_response - response_started);
  UMA_HISTOGRAM_TIMES(
      "Navigation.ResourceHandler."
      "ProceedWithResponseUntilFirstReadCompleted",
      first_read_completed - proceed_with_response);
}

}  // namespace content
