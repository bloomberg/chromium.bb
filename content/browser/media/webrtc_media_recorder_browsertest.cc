// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "media/base/media_switches.h"

#if defined(OS_ANDROID)
// TODO(cpaulin): when crbug.com/561068 is fixed, enable this test
// on android platform.
#define MAYBE_WebRtcMediaRecorderTest DISABLED_WebRtcMediaRecorderTest
#else
#define MAYBE_WebRtcMediaRecorderTest WebRtcMediaRecorderTest
#endif

namespace {

// Blink features necessary to run the test.
static const char kBlinkFeaturesNeeded[] = "GetUserMedia,MediaRecorder";

static const char kMediaRecorderHtmlFile[] = "/media/mediarecorder_test.html";

}  // namespace

namespace content {
// This class tests the recording of a media stream.
class MAYBE_WebRtcMediaRecorderTest : public WebRtcContentBrowserTest {
 public:
  MAYBE_WebRtcMediaRecorderTest() {}
  ~MAYBE_WebRtcMediaRecorderTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTest::SetUpCommandLine(command_line);

    // Turn on the flags we need.
    AppendUseFakeUIForMediaStreamFlag();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, kBlinkFeaturesNeeded);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_WebRtcMediaRecorderTest);
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest, MediaRecorderStart) {
  MakeTypicalCall("testStartAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderStartAndStop) {
  MakeTypicalCall("testStartStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderStartAndDataAvailable) {
  MakeTypicalCall("testStartAndDataAvailable();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderStartWithTimeSlice) {
  MakeTypicalCall("testStartWithTimeSlice();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest, MediaRecorderStartEvent) {
  MakeTypicalCall("testStartAndStartEventTriggered();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderStopEvent) {
  MakeTypicalCall("testStartStopAndStopEventTriggered();",
      kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest, MediaRecorderResume) {
  MakeTypicalCall("testResumeAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderResumeEvent) {
  MakeTypicalCall("testResumeAndResumeEventTriggered();",
      kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderNoResumeWhenRecorderInactive) {
  MakeTypicalCall("testNoResumeWhileRecorderInactive();",
      kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderResumeAndDataAvailable) {
  MakeTypicalCall("testResumeAndDataAvailable();", kMediaRecorderHtmlFile);
}

}  // namespace content
