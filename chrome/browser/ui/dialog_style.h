// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DIALOG_STYLE_H_
#define CHROME_BROWSER_UI_DIALOG_STYLE_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"

// A class to share common definitions between GTK and Views implementations.
enum DialogStyle {
  // Default style.
  STYLE_GENERIC = 0,

#if defined(OS_CHROMEOS)
  // Show close button at the top right (left for RTL).
  // Deprecated, see BubbleWindow::Create().
  // TODO(bshe): We probably need to use this style for certificate viewer
  // HTML dialog.
  STYLE_XBAR = 1 << 0,

  // Show throbber for slow rendering.
  // Deprecated, see BubbleWindow::Create().
  STYLE_THROBBER = 1 << 1,

  // Content and title flush to edge, no padding.
  STYLE_FLUSH = 1 << 2,

  // Content flush to edge. Padding only on title.
  STYLE_FLUSH_CONTENT = 1 << 3
#endif

};

#endif  // CHROME_BROWSER_UI_DIALOG_STYLE_H_
