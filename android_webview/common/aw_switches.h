// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
#define ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_

namespace switches {

// When set, falls back to using the old disk cache.
extern const char kDisableSimpleCache[];

// When set, forces use of fallback SW path even on HW canvas.
extern const char kDisableWebViewGLMode[];

// Used to make tile memory allocation decision. Roughly the number of
// displays that a single layer will have enough memory for.
extern const char kTileMemoryMultiplier[];

// Maximum number of gralloc allocations per webview.
extern const char kNumGrallocBuffersPerWebview[];

// Explicitly enable accelerated 2d canvas.
// TODO(boliu): Remove this switch once on by default.
extern const char kEnableAccelerated2dCanvas[];

}  // namespace switches

#endif  // ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
