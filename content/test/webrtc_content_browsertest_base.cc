// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/webrtc_content_browsertest_base.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"

namespace content {

void WebRtcContentBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  // We need fake devices in this test since we want to run on naked VMs. We
  // assume these switches are set by default in content_browsertests.
  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream));
  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeUIForMediaStream));

  // Always include loopback interface in network list, in case the test device
  // doesn't have other interfaces available.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLoopbackInPeerConnection);
}

void WebRtcContentBrowserTest::SetUp() {
  EnablePixelOutput();
  ContentBrowserTest::SetUp();
}

// Executes |javascript|. The script is required to use
// window.domAutomationController.send to send a string value back to here.
std::string WebRtcContentBrowserTest::ExecuteJavascriptAndReturnResult(
    const std::string& javascript) {
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(shell()->web_contents(),
                                            javascript,
                                            &result));
  return result;
}

void WebRtcContentBrowserTest::ExecuteJavascriptAndWaitForOk(
    const std::string& javascript) {
   std::string result = ExecuteJavascriptAndReturnResult(javascript);
   if (result != "OK") {
     printf("From javascript: %s", result.c_str());
     FAIL();
   }
 }

std::string WebRtcContentBrowserTest::GenerateGetUserMediaCall(
    const char* function_name,
    int min_width,
    int max_width,
    int min_height,
    int max_height,
    int min_frame_rate,
    int max_frame_rate) const {
  return base::StringPrintf(
      "%s({video: {mandatory: {minWidth: %d, maxWidth: %d, "
      "minHeight: %d, maxHeight: %d, minFrameRate: %d, maxFrameRate: %d}, "
      "optional: []}});",
      function_name,
      min_width,
      max_width,
      min_height,
      max_height,
      min_frame_rate,
      max_frame_rate);
}

}  // namespace content
