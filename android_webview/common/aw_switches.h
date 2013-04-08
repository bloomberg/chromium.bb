// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
#define ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_

namespace switches {

// Merge the Browser UI and the renderer compositor threads.
extern const char kMergeUIAndRendererCompositorThreads[];

// Uses zero-copy buffers in graphics pipeline.
extern const char kUseZeroCopyBuffers[];

}  // namespace switches

#endif  // ANDROID_WEBVIEW_COMMON_AW_SWITCHES_H_
