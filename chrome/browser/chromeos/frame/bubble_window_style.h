// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_STYLE_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_STYLE_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"

namespace chromeos {

// A class to share common definitions between GTK and Views implementations.
enum BubbleWindowStyle {
  // Default style.
  STYLE_GENERIC = 0,

  // Show close button at the top right (left for RTL).
  // Deprecated, see BubbleWindow::Create().
  STYLE_XBAR = 1 << 0,

  // Show throbber for slow rendering.
  // Deprecated, see BubbleWindow::Create().
  STYLE_THROBBER = 1 << 1,

  // Content flush to edge, no padding.
  STYLE_FLUSH = 1 << 2
};

extern const SkColor kBubbleWindowBackgroundColor;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_WINDOW_STYLE_H_
