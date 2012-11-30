// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/test_server.h"

namespace content {

// Tests The WebRTC getUserMedia call.
// Note: Requires --use-fake-device-for-media-stream (this flag is set by
// default in content_browsertests).
class WebrtcGetUserMediaTest: public ContentBrowserTest {
 public:
  WebrtcGetUserMediaTest() {}
 protected:
  void TestGetUserMediaWithConstraints(const std::string& constraints) {
    ASSERT_TRUE(test_server()->Start());
    GURL empty_url(test_server()->GetURL(
        "files/media/getusermedia_and_stop.html"));
    NavigateToURL(shell(), empty_url);

    RenderViewHost* render_view_host =
        shell()->web_contents()->GetRenderViewHost();

    EXPECT_TRUE(ExecuteJavaScript(render_view_host, L"",
                                  ASCIIToWide(constraints)));

    ExpectTitle("OK");
  }

  void ExpectTitle(const std::string& expected_title) const {
    string16 expected_title16(ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }
};

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.
IN_PROC_BROWSER_TEST_F(WebrtcGetUserMediaTest, GetVideoStreamAndStop) {
  TestGetUserMediaWithConstraints("getUserMedia({video: true});");
}

IN_PROC_BROWSER_TEST_F(WebrtcGetUserMediaTest, GetAudioAndVideoStreamAndStop) {
  TestGetUserMediaWithConstraints("getUserMedia({video: true, audio: true});");
}

// TODO(phoglund): enable once we have fake audio device support.
IN_PROC_BROWSER_TEST_F(WebrtcGetUserMediaTest, DISABLED_GetAudioStreamAndStop) {
  TestGetUserMediaWithConstraints("getUserMedia({audio: true});");
}

}  // namespace content

