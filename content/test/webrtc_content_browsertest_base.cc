// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/webrtc_content_browsertest_base.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils.h"

namespace content {

void WebRtcContentBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  // We need fake devices in this test since we want to run on naked VMs. We
  // assume these switches are set by default in content_browsertests.
  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream));
  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeUIForMediaStream));
}

void WebRtcContentBrowserTest::SetUp() {
  EnablePixelOutput();
  ContentBrowserTest::SetUp();
}

bool WebRtcContentBrowserTest::ExecuteJavascript(
    const std::string& javascript) {
  return ExecuteScript(shell()->web_contents(), javascript);
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

void WebRtcContentBrowserTest::ExpectTitle(
    const std::string& expected_title) const {
  base::string16 expected_title16(base::ASCIIToUTF16(expected_title));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
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
