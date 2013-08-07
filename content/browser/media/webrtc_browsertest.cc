// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

std::string GenerateGetUserMediaCall(int min_width,
                                     int max_width,
                                     int min_height,
                                     int max_height,
                                     int min_frame_rate,
                                     int max_frame_rate) {
  return base::StringPrintf(
      "getUserMedia({video: {mandatory: {minWidth: %d, maxWidth: %d, "
      "minHeight: %d, maxHeight: %d, minFrameRate: %d, maxFrameRate: %d}, "
      "optional: []}});",
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

  virtual void SetUpOnMainThread() OVERRIDE {
    // We need fake devices in this test since we want to run on naked VMs. We
    // assume these switches are set by default in content_browsertests.
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeUIForMediaStream));

    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
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
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("getUserMedia({video: true});"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetAudioAndVideoStreamAndStop) {
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("getUserMedia({video: true, audio: true});"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetAudioAndVideoStreamAndClone) {
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
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("call({video: true, audio: true});"));
  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MANUAL_CanSetupCallAndSendDtmf) {
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(
      ExecuteJavascript("callAndSendDtmf('123,abc');"));
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       CanMakeEmptyCallThenAddStreamsAndRenegotiate) {
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  const char* kJavascript =
      "callEmptyThenAddOneStreamAndRenegotiate({video: true, audio: true});";
  EXPECT_TRUE(ExecuteJavascript(kJavascript));
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
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithoutMsidAndBundle();"));
  ExpectTitle("OK");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallWithDataOnly) {
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataOnly();"));
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
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithDataAndMedia) {
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("callWithDataAndMedia();"));
  ExpectTitle("OK");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithDataAndLaterAddMedia DISABLED_CallWithDataAndLaterAddMedia
#else
#define MAYBE_CallWithDataAndLaterAddMedia CallWithDataAndLaterAddMedia
#endif

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and later add an audio and video track.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithDataAndLaterAddMedia) {
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
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(
      ExecuteJavascript("callWithNewVideoMediaStreamLaterSwitchToAudio();"));
  ExpectTitle("OK");
}

// This test calls getUserMedia in sequence with different constraints.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, TestGetUserMediaConstraints) {
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::vector<std::string> list_of_get_user_media_calls;
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(320, 320, 180, 180, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(320, 320, 240, 240, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(640, 640, 360, 360, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(640, 640, 480, 480, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(960, 960, 720, 720, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(1280, 1280, 720, 720, 30, 30));
  list_of_get_user_media_calls.push_back(
      GenerateGetUserMediaCall(1920, 1920, 1080, 1080, 30, 30));

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

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, AddTwoMediaStreamsToOnePC) {
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(
      ExecuteJavascript("addTwoMediaStreamsToOneConnection();"));
  ExpectTitle("OK");
}

}  // namespace content
