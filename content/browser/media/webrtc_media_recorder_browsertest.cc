// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
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

static const char kBlinkFeaturesNeeded[] = "GetUserMedia";

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

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest, MediaRecorderResume) {
  MakeTypicalCall("testResumeAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderNoResumeWhenRecorderInactive) {
  MakeTypicalCall("testIllegalResumeThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderResumeAndDataAvailable) {
  MakeTypicalCall("testResumeAndDataAvailable();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderPause) {
  MakeTypicalCall("testPauseAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderPauseStop) {
  MakeTypicalCall("testPauseStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderPausePreventsDataavailableFromBeingFired) {
  MakeTypicalCall("testPausePreventsDataavailableFromBeingFired();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderIllegalPauseThrowsDOMError) {
  MakeTypicalCall("testIllegalPauseThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderTwoChannelAudioRecording) {
  MakeTypicalCall("testTwoChannelAudio();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderIllegalStopThrowsDOMError) {
  MakeTypicalCall("testIllegalStopThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderIllegalStartWhileRecordingThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInRecordingStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderIllegalStartWhilePausedThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInPausedStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderIllegalRequestDataThrowsDOMError) {
  MakeTypicalCall("testIllegalRequestDataThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       MediaRecorderPeerConnection) {
  MakeTypicalCall("testRecordRemotePeerConnection();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       AddingTrackToMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testAddingTrackToMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcMediaRecorderTest,
                       RemovingTrackFromMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testRemovingTrackFromMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

}  // namespace content
