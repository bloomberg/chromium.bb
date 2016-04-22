// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/test/webrtc_content_browsertest_base.h"

namespace {

static const char kCanvasTestHtmlPage[] = "/media/canvas_capture_color.html";

}  // namespace

namespace content {

// This class tests the rgba color values produced by canvas capture.
class WebRtcCanvasCaptureTest : public WebRtcContentBrowserTest {
 public:
  WebRtcCanvasCaptureTest() {}
  ~WebRtcCanvasCaptureTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcCanvasCaptureTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcCanvasCaptureTest,
    VerifyCanvasCaptureColor) {
  MakeTypicalCall("testCanvasCaptureColors();", kCanvasTestHtmlPage);
}

}  // namespace content
