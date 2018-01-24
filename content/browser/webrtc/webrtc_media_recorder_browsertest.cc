// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/media_switches.h"

namespace {

static const char kMediaRecorderHtmlFile[] = "/media/mediarecorder_test.html";

static struct EncodingParameters {
  bool disable_accelerator;
  std::string mime_type;
} const kEncodingParameters[] = {
    {true, "video/webm;codecs=VP8"},
    {true, "video/webm;codecs=VP9"},
    {true, "video/x-matroska;codecs=AVC1"},
    {false, ""},  // Instructs the platform to choose any accelerated codec.
    {false, "video/webm;codecs=VP8"},
    {false, "video/webm;codecs=VP9"},
    {false, "video/x-matroska;codecs=AVC1"},
};

}  // namespace

namespace content {

// This class tests the recording of a media stream.
class WebRtcMediaRecorderTest
    : public WebRtcContentBrowserTestBase,
      public testing::WithParamInterface<struct EncodingParameters> {
 public:
  WebRtcMediaRecorderTest() {}
  ~WebRtcMediaRecorderTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);

    AppendUseFakeUIForMediaStreamFlag();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "GetUserMedia");
  }

  void MaybeForceDisableEncodeAccelerator(bool disable) {
    if (!disable)
      return;
    // This flag is also used for encoding, https://crbug.com/616640.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableAcceleratedVideoDecode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcMediaRecorderTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, Start) {
  MakeTypicalCall("testStartAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, StartAndStop) {
  MakeTypicalCall("testStartStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, StartAndDataAvailable) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testStartAndDataAvailable(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

// TODO(crbug.com/805341): It seems to be flaky on Android. More details in
// the bug.
#if defined(OS_ANDROID)
#define MAYBE_StartWithTimeSlice DISABLED_StartWithTimeSlice
#else
#define MAYBE_StartWithTimeSlice StartWithTimeSlice
#endif
IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_StartWithTimeSlice) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testStartWithTimeSlice(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, Resume) {
  MakeTypicalCall("testResumeAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, NoResumeWhenRecorderInactive) {
  MakeTypicalCall("testIllegalResumeThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, ResumeAndDataAvailable) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testResumeAndDataAvailable(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, Pause) {
  MakeTypicalCall("testPauseAndRecorderState();", kMediaRecorderHtmlFile);
}

// TODO(crbug.com/571389): Flaky on TSAN bots.
#if defined(OS_LINUX)
#define MAYBE_PauseStop DISABLED_PauseStop
#else
#define MAYBE_PauseStop PauseStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, MAYBE_PauseStop) {
  MakeTypicalCall("testPauseStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       PausePreventsDataavailableFromBeingFired) {
  MakeTypicalCall("testPausePreventsDataavailableFromBeingFired();",
                  kMediaRecorderHtmlFile);
}

// TODO (crbug.com/736268): Flaky on Linux TSan bots.
#if defined(OS_LINUX)
#define MAYBE_IllegalPauseThrowsDOMError DISABLED_IllegalPauseThrowsDOMError
#else
#define MAYBE_IllegalPauseThrowsDOMError IllegalPauseThrowsDOMError
#endif
IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MAYBE_IllegalPauseThrowsDOMError) {
  MakeTypicalCall("testIllegalPauseThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, TwoChannelAudioRecording) {
  MakeTypicalCall("testTwoChannelAudio();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, RecordWithTransparency) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testRecordWithTransparency(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, IllegalStopThrowsDOMError) {
  MakeTypicalCall("testIllegalStopThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       IllegalStartWhileRecordingThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInRecordingStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       IllegalStartWhilePausedThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInPausedStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

// Flaky on Linux Tsan (crbug.com/736268)
#if defined(OS_LINUX)
#define MAYBE_IllegalRequestDataThrowsDOMError \
  DISABLED_IllegalRequestDataThrowsDOMError
#else
#define MAYBE_IllegalRequestDataThrowsDOMError IllegalRequestDataThrowsDOMError
#endif
IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MAYBE_IllegalRequestDataThrowsDOMError) {
  MakeTypicalCall("testIllegalRequestDataThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

#if defined(OS_ANDROID)
// These tests are flakily timing out on emulators (https://crbug.com/716691)
// and/or under Android ASAN (https://crbug.com/693565);
#define MAYBE_PeerConnection DISABLED_PeerConnection
#elif defined(OS_LINUX) && defined(THREAD_SANITIZER)
// Flaky on Linux TSan, https://crbug.com/694373.
#define MAYBE_PeerConnection DISABLED_PeerConnection
#elif defined(OS_WIN) && !defined(NDEBUG)
// Fails on Win7 debug, https://crbug.com/703844.
#define MAYBE_PeerConnection DISABLED_PeerConnection
#else
#define MAYBE_PeerConnection PeerConnection
#endif

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, MAYBE_PeerConnection) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testRecordRemotePeerConnection(\"%s\");",
                                     GetParam().mime_type.c_str()),
                  kMediaRecorderHtmlFile);
}

// Flaky on Linux Tsan (crbug.com/736268)
#if defined(OS_LINUX)
#define MAYBE_AddingTrackToMediaStreamFiresErrorEvent \
  DISABLED_AddingTrackToMediaStreamFiresErrorEvent
#else
#define MAYBE_AddingTrackToMediaStreamFiresErrorEvent \
  AddingTrackToMediaStreamFiresErrorEvent
#endif
IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       MAYBE_AddingTrackToMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testAddingTrackToMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       RemovingTrackFromMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testRemovingTrackFromMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

INSTANTIATE_TEST_CASE_P(,
                        WebRtcMediaRecorderTest,
                        testing::ValuesIn(kEncodingParameters));

}  // namespace content
