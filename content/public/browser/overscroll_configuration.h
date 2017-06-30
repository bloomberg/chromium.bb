// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
#define CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_

#include "content/common/content_export.h"

namespace content {

// Sets and retrieves various overscroll related configuration values.
enum OverscrollConfig {
  OVERSCROLL_CONFIG_NONE,

  // Threshold to complete horizontal overscroll. For touchpad, it represents
  // the percentage of the display width. For touchscreen, it represents the
  // percentage of the window width.
  OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE,

  // Threshold to complete vertical overscroll. For touchpad, it represents the
  // percentage of the display width. For touchscreen, it represents the
  // percentage of the window width.
  OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE,

  // Threshold to start horizontal touchpad overscroll, in DIPs.
  OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD,

  // Threshold to start horizontal touchscreen overscroll, in DIPs.
  OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN,

  // Threshold to start vertical overscroll, in DIPs.
  OVERSCROLL_CONFIG_VERT_THRESHOLD_START,
};

CONTENT_EXPORT float GetOverscrollConfig(OverscrollConfig config);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OVERSCROLL_CONFIGURATION_H_
