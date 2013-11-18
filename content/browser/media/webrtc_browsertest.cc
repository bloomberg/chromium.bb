// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

static const char kGetUserMedia[] = "getUserMedia";
static const char kGetUserMediaWithAnalysis[] = "getUserMediaWithAnalysis";

std::string GenerateGetUserMediaCall(const char* function_name,
                                     int min_width,
                                     int max_width,
                                     int min_height,
                                     int max_height,
                                     int min_frame_rate,
                                     int max_frame_rate) {
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
}

namespace content {

class WebrtcBrowserTest: public ContentBrowserTest {
 public:
  WebrtcBrowserTest() {}
  virtual ~WebrtcBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // We need fake devices in this test since we want to run on naked VMs. We
    // assume these switches are set by default in content_browsertests.
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    // This may not be available on all VMs though.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

 protected:
  bool ExecuteJavascript(const std::string& javascript) {
    return ExecuteScript(shell()->web_contents(), javascript);
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
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("getUserMedia({video: true});"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetAudioAndVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("getUserMedia({video: true, audio: true});"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetAudioAndVideoStreamAndClone) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("getUserMediaAndClone();"));

  ExpectTitle("OK");
}


#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CanSetupVideoCall DISABLED_CanSetupVideoCall
#else
#define MAYBE_CanSetupVideoCall CanSetupVideoCall
#endif

// These tests will make a complete PeerConnection-based call and verify that
// video is playing for the call.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanSetupVideoCall) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("call({video: true});"));
  ExpectTitle("OK");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240376
#define MAYBE_CanSetupAudioAndVideoCall DISABLED_CanSetupAudioAndVideoCall
#else
#define MAYBE_CanSetupAudioAndVideoCall CanSetupAudioAndVideoCall
#endif

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanSetupAudioAndVideoCall) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("call({video: true, audio: true});"));
  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MANUAL_CanSetupCallAndSendDtmf) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(
      ExecuteJavascript("callAndSendDtmf('123,abc');"));
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       DISABLED_CanMakeEmptyCallThenAddStreamsAndRenegotiate) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  const char* kJavascript =
      "callEmptyThenAddOneStreamAndRenegotiate({video: true, audio: true});";
  EXPECT_TRUE(ExecuteJavascript(kJavascript));
  ExpectTitle("OK");
}

// Below 2 test will make a complete PeerConnection-based call between pc1 and
// pc2, and then use the remote stream to setup a call between pc3 and pc4, and
// then verify that video is received on pc3 and pc4.
// Flaky on win xp. http://crbug.com/304775
#if defined(OS_WIN)
#define MAYBE_CanForwardRemoteStream DISABLED_CanForwardRemoteStream
#define MAYBE_CanForwardRemoteStream720p DISABLED_CanForwardRemoteStream720p
#else
#define MAYBE_CanForwardRemoteStream CanForwardRemoteStream
#define MAYBE_CanForwardRemoteStream720p CanForwardRemoteStream720p
#endif
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanForwardRemoteStream) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript(
                  "callAndForwardRemoteStream({video: true, audio: true});"));
  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanForwardRemoteStream720p) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  const std::string cmd = GenerateGetUserMediaCall("callAndForwardRemoteStream",
                                                   1280, 1280,
                                                   720, 720, 30, 30);
  EXPECT_TRUE(ExecuteJavascript(cmd));
  ExpectTitle("OK");
}

// This test will make a complete PeerConnection-based call but remove the
// MSID and bundle attribute from the initial offer to verify that
// video is playing for the call even if the initiating client don't support
// MSID. http://tools.ietf.org/html/draft-alvestrand-rtcweb-msid-02
#if defined(OS_WIN) && defined(USE_AURA)
// Disabled for win7_aura, see http://crbug.com/235089.
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        DISABLED_CanSetupAudioAndVideoCallWithoutMsidAndBundle
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240373
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        DISABLED_CanSetupAudioAndVideoCallWithoutMsidAndBundle
#else
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        CanSetupAudioAndVideoCallWithoutMsidAndBundle
#endif
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithoutMsidAndBundle();"));
  ExpectTitle("OK");
}

// This test will modify the SDP offer to an unsupported codec, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       NegotiateUnsupportedVideoCodec) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("negotiateUnsupportedVideoCodec();"));
  ExpectTitle("OK");
}

// This test will modify the SDP offer to use no encryption, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, NegotiateNonCryptoCall) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("negotiateNonCryptoCall();"));
  ExpectTitle("OK");
}

// This test will make a complete PeerConnection-based call using legacy SDP
// settings: GIce, external SDES, and no BUNDLE.
#if defined(OS_WIN) && defined(USE_AURA)
// Disabled for win7_aura, see http://crbug.com/235089.
#define MAYBE_CanSetupLegacyCall DISABLED_CanSetupLegacyCall
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240373
#define MAYBE_CanSetupLegacyCall DISABLED_CanSetupLegacyCall
#else
#define MAYBE_CanSetupLegacyCall CanSetupLegacyCall
#endif

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanSetupLegacyCall) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithLegacySdp();"));
  ExpectTitle("OK");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallWithDataOnly) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataOnly();"));
  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallWithSctpDataOnly) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithSctpDataOnly();"));
  ExpectTitle("OK");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithDataAndMedia DISABLED_CallWithDataAndMedia
#else
#define MAYBE_CallWithDataAndMedia CallWithDataAndMedia
#endif

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and audio and video tracks.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithDataAndMedia) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataAndMedia();"));
  ExpectTitle("OK");
}


#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithSctpDataAndMedia DISABLED_CallWithSctpDataAndMedia
#else
#define MAYBE_CallWithSctpDataAndMedia CallWithSctpDataAndMedia
#endif

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MAYBE_CallWithSctpDataAndMedia) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithSctpDataAndMedia();"));
  ExpectTitle("OK");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithDataAndLaterAddMedia DISABLED_CallWithDataAndLaterAddMedia
#else
// Temporarily disable the test on all platforms. http://crbug.com/293252
#define MAYBE_CallWithDataAndLaterAddMedia DISABLED_CallWithDataAndLaterAddMedia
#endif

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and later add an audio and video track.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithDataAndLaterAddMedia) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataAndLaterAddMedia();"));
  ExpectTitle("OK");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithNewVideoMediaStream DISABLED_CallWithNewVideoMediaStream
#else
#define MAYBE_CallWithNewVideoMediaStream CallWithNewVideoMediaStream
#endif

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithNewVideoMediaStream) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithNewVideoMediaStream();"));
  ExpectTitle("OK");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia. When video is flowing, the VideoTrack is removed and an
// AudioTrack is added instead.
// TODO(phoglund): This test is manual since not all buildbots has an audio
// input.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MANUAL_CallAndModifyStream) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(
      ExecuteJavascript("callWithNewVideoMediaStreamLaterSwitchToAudio();"));
  ExpectTitle("OK");
}

// This test calls getUserMedia in sequence with different constraints.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, TestGetUserMediaConstraints) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::vector<std::string> list_of_get_user_media_calls;
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(kGetUserMedia, 320, 320, 180, 180, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(kGetUserMedia, 320, 320, 240, 240, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(kGetUserMedia, 640, 640, 360, 360, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(kGetUserMedia, 640, 640, 480, 480, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(kGetUserMedia, 960, 960, 720, 720, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(kGetUserMedia, 1280, 1280, 720, 720, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(kGetUserMedia, 1920, 1920, 1080, 1080, 30, 30));

  for (std::vector<std::string>::iterator const_iterator =
           list_of_get_user_media_calls.begin();
       const_iterator != list_of_get_user_media_calls.end();
       ++const_iterator) {
    DVLOG(1) << "Calling getUserMedia: " << *const_iterator;
    NavigateToURL(shell(), url);
    EXPECT_TRUE(ExecuteJavascript(*const_iterator));
    ExpectTitle("OK");
  }
}

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, TestGetUserMediaAspectRatio) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_4_3 = GenerateGetUserMediaCall(
      kGetUserMediaWithAnalysis, 640, 640, 480, 480, 30, 30);
  std::string constraints_16_9 = GenerateGetUserMediaCall(
      kGetUserMediaWithAnalysis, 640, 640, 360, 360, 30, 30);

  // TODO(mcasas): add more aspect ratios, in particular 16:10 crbug.com/275594.

  NavigateToURL(shell(), url);
  EXPECT_TRUE(ExecuteJavascript(constraints_4_3));
  ExpectTitle("4:3 letterbox");

  NavigateToURL(shell(), url);
  EXPECT_TRUE(ExecuteJavascript(constraints_16_9));
  ExpectTitle("16:9 letterbox");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, AddTwoMediaStreamsToOnePC) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(
      ExecuteJavascript("addTwoMediaStreamsToOneConnection();"));
  ExpectTitle("OK");
}

}  // namespace content
