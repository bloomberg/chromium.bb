// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
#define CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_

#include "content/common/content_export.h"

namespace content {

// Sets and retrieves various overscroll related configuration values.
enum class OverscrollConfig {
  // Threshold to complete touchpad overscroll, in terms of the percentage of
  // the display size.
  THRESHOLD_COMPLETE_TOUCHPAD,

  // Threshold to complete touchscreen overscroll, in terms of the percentage of
  // the display size.
  THRESHOLD_COMPLETE_TOUCHSCREEN,

  // Threshold to start touchpad overscroll, in DIPs.
  THRESHOLD_START_TOUCHPAD,

  // Threshold to start touchscreen overscroll, in DIPs.
  THRESHOLD_START_TOUCHSCREEN,
};

CONTENT_EXPORT float GetOverscrollConfig(OverscrollConfig config);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
