// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_PIXEL_DUMP_H_
#define CONTENT_SHELL_TEST_RUNNER_PIXEL_DUMP_H_

#include "base/callback_forward.h"
#include "content/shell/test_runner/test_runner_export.h"

class SkBitmap;

namespace blink {
class WebView;
}  // namespace blink

namespace test_runner {

class LayoutTestRuntimeFlags;

// Dumps image snapshot of |web_view|.  Exact dump mode depends on |flags| (i.e.
// dump_selection_rect and/or is_printing).  Caller needs to ensure that
// |layout_test_runtime_flags| stays alive until |callback| gets called.
TEST_RUNNER_EXPORT void DumpPixelsAsync(
    blink::WebView* web_view,
    const LayoutTestRuntimeFlags& layout_test_runtime_flags,
    float device_scale_factor_for_test,
    const base::Callback<void(const SkBitmap&)>& callback);

// Copy to clipboard the image present at |x|, |y| coordinates in |web_view|
// and pass the captured image to |callback|.
void CopyImageAtAndCapturePixels(
    blink::WebView* web_view,
    int x,
    int y,
    const base::Callback<void(const SkBitmap&)>& callback);

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_PIXEL_DUMP_H_
