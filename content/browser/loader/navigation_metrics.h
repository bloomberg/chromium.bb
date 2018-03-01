// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_METRICS_H
#define CONTENT_BROWSER_LOADER_NAVIGATION_METRICS_H

#include "base/time/time.h"

namespace content {

// Records histograms for the time spent between several events in the
// ResourceHandler dedicated to navigations.
// - |response_started| is when the response's headers and metadata are
//   available. Loading is paused at this time.
// - |proceed_with_response| is when loading is resumed.
// - |first_read_completed| is when the first part of the body has been read.
//
// Depending on whether NavigationMojoResponse is enabled or not, these times
// are either recorded by the MojoAsyncResourceHandler or the
// NavigationResourceHandler.
//
// TODO(arthursonzogni): Move this function in MojoAsyncResourceHandler once
// NavigationResourceHandler has been deleted.
void RecordNavigationResourceHandlerMetrics(
    base::TimeTicks response_started,
    base::TimeTicks proceed_with_response,
    base::TimeTicks first_read_completed);

}  // namespace content.

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_METRICS_H
