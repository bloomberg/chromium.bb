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

namespace {

static const char kMediaRecorderHtmlFile[] = "/media/mediarecorder_test.html";

}  // namespace

namespace content {

// This class tests the recording of a media stream.
class WebRtcMediaRecorderTest : public WebRtcContentBrowserTest {
 public:
  WebRtcMediaRecorderTest() {}
  ~WebRtcMediaRecorderTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTest::SetUpCommandLine(command_line);

    AppendUseFakeUIForMediaStreamFlag();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "GetUserMedia");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcMediaRecorderTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, MediaRecorderStart) {
  MakeTypicalCall("testStartAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, MediaRecorderStartAndStop) {
  MakeTypicalCall("testStartStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderStartAndDataAvailable) {
  MakeTypicalCall("testStartAndDataAvailable();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderStartWithTimeSlice) {
  MakeTypicalCall("testStartWithTimeSlice();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, MediaRecorderResume) {
  MakeTypicalCall("testResumeAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderNoResumeWhenRecorderInactive) {
  MakeTypicalCall("testIllegalResumeThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderResumeAndDataAvailable) {
  MakeTypicalCall("testResumeAndDataAvailable();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, MediaRecorderPause) {
  MakeTypicalCall("testPauseAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, MediaRecorderPauseStop) {
  MakeTypicalCall("testPauseStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderPausePreventsDataavailableFromBeingFired) {
  MakeTypicalCall("testPausePreventsDataavailableFromBeingFired();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderIllegalPauseThrowsDOMError) {
  MakeTypicalCall("testIllegalPauseThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderTwoChannelAudioRecording) {
  MakeTypicalCall("testTwoChannelAudio();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderIllegalStopThrowsDOMError) {
  MakeTypicalCall("testIllegalStopThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderIllegalStartWhileRecordingThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInRecordingStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderIllegalStartWhilePausedThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInPausedStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MediaRecorderIllegalRequestDataThrowsDOMError) {
  MakeTypicalCall("testIllegalRequestDataThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

#if defined(OS_ANDROID)
// TODO(cpaulin): when http://crbug.com/585242 is fixed, enable peer connection
// recording test on android platform.
#define MAYBE_MediaRecorderPeerConnection DISABLED_MediaRecorderPeerConnection
#else
#define MAYBE_MediaRecorderPeerConnection MediaRecorderPeerConnection
#endif

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MAYBE_MediaRecorderPeerConnection) {
  MakeTypicalCall("testRecordRemotePeerConnection();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       AddingTrackToMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testAddingTrackToMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       RemovingTrackFromMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testRemovingTrackFromMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

}  // namespace content
