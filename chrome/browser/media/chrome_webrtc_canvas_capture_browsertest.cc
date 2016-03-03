// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"
#include "ui/gl/gl_switches.h"

static const char kCanvasTestHtmlPage[] = "/media/canvas_capture_test.html";

#if defined(OS_MACOSX)
// TODO(cpaulin): when http://crbug.com/591529 is fixed on MAC OS, enable this
// test.
#define MAYBE_WebRtcCanvasCaptureBrowserTest \
    DISABLED_WebRtcCanvasCaptureBrowserTest
#else
#define MAYBE_WebRtcCanvasCaptureBrowserTest WebRtcCanvasCaptureBrowserTest
#endif

// Canvas Capture browser test.
// At this time this browser test verifies that the frame rate of a stream
// originated from a captured canvas is as expected.
class MAYBE_WebRtcCanvasCaptureBrowserTest : public InProcessBrowserTest {
 public:

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);

    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

 protected:
  MAYBE_WebRtcCanvasCaptureBrowserTest() {}
  ~MAYBE_WebRtcCanvasCaptureBrowserTest() override {};

  std::string ExecuteJavascript(
      const std::string& javascript,
      content::WebContents* tab_contents) const {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab_contents, javascript, &result));
    return result;
  }

 private:
   DISALLOW_COPY_AND_ASSIGN(MAYBE_WebRtcCanvasCaptureBrowserTest);
};

// Tests that the frame rate of the canvas capture is as expected.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcCanvasCaptureBrowserTest,
                       VerifyCanvasCaptureFrameRate) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kCanvasTestHtmlPage));

  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_EQ("OK", ExecuteJavascript("testFrameRateOfCanvasCapture();",
                                    tab_contents));
}
