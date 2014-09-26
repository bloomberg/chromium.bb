// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

#if defined (OS_ANDROID) || defined(THREAD_SANITIZER)
// Just do the bare minimum of audio checking on Android and under TSAN since
// it's a bit sensitive to device performance.
const char kUseLenientAudioChecking[] = "true";
#else
const char kUseLenientAudioChecking[] = "false";
#endif

}  // namespace

namespace content {

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcBrowserTest DISABLED_WebRtcBrowserTest
#elif defined(OS_ANDROID) && defined(__aarch64__)
// Failures on ARM64 Android: http://crbug.com/408179.
#define MAYBE_WebRtcBrowserTest DISABLED_WebRtcBrowserTest
#else
#define MAYBE_WebRtcBrowserTest WebRtcBrowserTest
#endif

class MAYBE_WebRtcBrowserTest : public WebRtcContentBrowserTest {
 public:
  MAYBE_WebRtcBrowserTest() {}
  virtual ~MAYBE_WebRtcBrowserTest() {}

  // Convenience function since most peerconnection-call.html tests just load
  // the page, kick off some javascript and wait for the title to change to OK.
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
    NavigateToURL(shell(), url);

    DisableOpusIfOnAndroid();
    ExecuteJavascriptAndWaitForOk(javascript);
  }

  // Convenience method for making calls that detect if audio os playing (which
  // has some special prerequisites, such that there needs to be an audio output
  // device on the executing machine).
  void MakeAudioDetectingPeerConnectionCall(const std::string& javascript) {
    if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
      // Bots with no output devices will force the audio code into a state
      // where it doesn't manage to set either the low or high latency path.
      // This test will compute useless values in that case, so skip running on
      // such bots (see crbug.com/326338).
      LOG(INFO) << "Missing output devices: skipping test...";
      return;
    }

    ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream))
            << "Must run with fake devices since the test will explicitly look "
            << "for the fake device signal.";

    MakeTypicalPeerConnectionCall(javascript);
  }
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CanSetupDefaultVideoCall DISABLED_CanSetupDefaultVideoCall
// Flaky on TSAN v2. http://crbug.com/408006
#elif defined(THREAD_SANITIZER)
#define MAYBE_CanSetupDefaultVideoCall DISABLED_CanSetupDefaultVideoCall
#else
#define MAYBE_CanSetupDefaultVideoCall CanSetupDefaultVideoCall
#endif

// These tests will make a complete PeerConnection-based call and verify that
// video is playing for the call.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupDefaultVideoCall) {
  MakeTypicalPeerConnectionCall(
      "callAndExpectResolution({video: true}, 640, 480);");
}

// Flaky on TSAN v2. http://crbug.com/408006
#if defined(THREAD_SANITIZER)
#define MAYBE_CanSetupVideoCallWith1To1AspectRatio \
  DISABLED_CanSetupVideoCallWith1To1AspectRatio
#else
#define MAYBE_CanSetupVideoCallWith1To1AspectRatio \
  CanSetupVideoCallWith1To1AspectRatio
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupVideoCallWith1To1AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 320,"
      " maxWidth: 320, minHeight: 320, maxHeight: 320}}}, 320, 320);";
  MakeTypicalPeerConnectionCall(javascript);
}

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARM64)
// Failing on ARM64 Android bot: http://crbug.com/408179
#define MAYBE_CanSetupVideoCallWith16To9AspectRatio \
  DISABLED_CanSetupVideoCallWith16To9AspectRatio
// Flaky on TSAN v2. http://crbug.com/408006
#elif defined(THREAD_SANITIZER)
#define MAYBE_CanSetupVideoCallWith16To9AspectRatio \
  DISABLED_CanSetupVideoCallWith16To9AspectRatio
#else
#define MAYBE_CanSetupVideoCallWith16To9AspectRatio \
  CanSetupVideoCallWith16To9AspectRatio
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupVideoCallWith16To9AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 640,"
      " maxWidth: 640, minAspectRatio: 1.777}}}, 640, 360);";
  MakeTypicalPeerConnectionCall(javascript);
}

// Flaky on TSAN v2. http://crbug.com/408006
#if defined(THREAD_SANITIZER)
#define MAYBE_CanSetupVideoCallWith4To3AspectRatio \
  DISABLED_CanSetupVideoCallWith4To3AspectRatio
#else
#define MAYBE_CanSetupVideoCallWith4To3AspectRatio \
  CanSetupVideoCallWith4To3AspectRatio
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupVideoCallWith4To3AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 960,"
      "maxAspectRatio: 1.333}}}, 960, 720);";
  MakeTypicalPeerConnectionCall(javascript);
}

// Flaky on TSAN v2. http://crbug.com/408006
#if defined(THREAD_SANITIZER)
#define MAYBE_CanSetupVideoCallAndDisableLocalVideo \
  DISABLED_CanSetupVideoCallAndDisableLocalVideo
#else
#define MAYBE_CanSetupVideoCallAndDisableLocalVideo \
  CanSetupVideoCallAndDisableLocalVideo
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupVideoCallAndDisableLocalVideo) {
  const std::string javascript =
      "callAndDisableLocalVideo({video: true});";
  MakeTypicalPeerConnectionCall(javascript);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240376
#define MAYBE_CanSetupAudioAndVideoCall DISABLED_CanSetupAudioAndVideoCall
#else
#define MAYBE_CanSetupAudioAndVideoCall CanSetupAudioAndVideoCall
#endif

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupAudioAndVideoCall) {
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MANUAL_CanSetupCallAndSendDtmf) {
  MakeTypicalPeerConnectionCall("callAndSendDtmf(\'123,abc\');");
}

// TODO(phoglund): this test fails because the peer connection state will be
// stable in the second negotiation round rather than have-local-offer.
// http://crbug.com/293125.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       DISABLED_CanMakeEmptyCallThenAddStreamsAndRenegotiate) {
  const char* kJavascript =
      "callEmptyThenAddOneStreamAndRenegotiate({video: true, audio: true});";
  MakeTypicalPeerConnectionCall(kJavascript);
}

// Below 2 test will make a complete PeerConnection-based call between pc1 and
// pc2, and then use the remote stream to setup a call between pc3 and pc4, and
// then verify that video is received on pc3 and pc4.
// The stream sent from pc3 to pc4 is the stream received on pc1.
// The stream sent from pc4 to pc3 is cloned from stream the stream received
// on pc2.
// Flaky on win xp. http://crbug.com/304775
#if defined(OS_WIN)
#define MAYBE_CanForwardRemoteStream DISABLED_CanForwardRemoteStream
#define MAYBE_CanForwardRemoteStream720p DISABLED_CanForwardRemoteStream720p
#else
#define MAYBE_CanForwardRemoteStream CanForwardRemoteStream
// Flaky on TSAN v2. http://crbug.com/373637
#if defined(THREAD_SANITIZER)
#define MAYBE_CanForwardRemoteStream720p DISABLED_CanForwardRemoteStream720p
#else
#define MAYBE_CanForwardRemoteStream720p CanForwardRemoteStream720p
#endif
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, MAYBE_CanForwardRemoteStream) {
#if defined (OS_ANDROID)
  // This test fails on Nexus 5 devices.
  // TODO(henrika): see http://crbug.com/362437 and http://crbug.com/359389
  // for details.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebRtcHWDecoding);
#endif
  MakeTypicalPeerConnectionCall(
      "callAndForwardRemoteStream({video: true, audio: false});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanForwardRemoteStream720p) {
#if defined (OS_ANDROID)
  // This test fails on Nexus 5 devices.
  // TODO(henrika): see http://crbug.com/362437 and http://crbug.com/359389
  // for details.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebRtcHWDecoding);
#endif
  const std::string javascript = GenerateGetUserMediaCall(
      "callAndForwardRemoteStream", 1280, 1280, 720, 720, 10, 30);
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       NoCrashWhenConnectChromiumSinkToRemoteTrack) {
  MakeTypicalPeerConnectionCall("ConnectChromiumSinkToRemoteAudioTrack();");
}

// This test will make a complete PeerConnection-based call but remove the
// MSID and bundle attribute from the initial offer to verify that
// video is playing for the call even if the initiating client don't support
// MSID. http://tools.ietf.org/html/draft-alvestrand-rtcweb-msid-02
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240373
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        DISABLED_CanSetupAudioAndVideoCallWithoutMsidAndBundle
#else
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        CanSetupAudioAndVideoCallWithoutMsidAndBundle
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle) {
  MakeTypicalPeerConnectionCall("callWithoutMsidAndBundle();");
}

// This test will modify the SDP offer to an unsupported codec, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       NegotiateUnsupportedVideoCodec) {
  MakeTypicalPeerConnectionCall("negotiateUnsupportedVideoCodec();");
}

// This test will modify the SDP offer to use no encryption, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, NegotiateNonCryptoCall) {
  MakeTypicalPeerConnectionCall("negotiateNonCryptoCall();");
}

// This test can negotiate an SDP offer that includes a b=AS:xx to control
// the bandwidth for audio and video
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, NegotiateOfferWithBLine) {
  MakeTypicalPeerConnectionCall("negotiateOfferWithBLine();");
}

// This test will make a complete PeerConnection-based call using legacy SDP
// settings: GIce, external SDES, and no BUNDLE.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240373
#define MAYBE_CanSetupLegacyCall DISABLED_CanSetupLegacyCall
#else
#define MAYBE_CanSetupLegacyCall CanSetupLegacyCall
#endif

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, MAYBE_CanSetupLegacyCall) {
  MakeTypicalPeerConnectionCall("callWithLegacySdp();");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallWithDataOnly) {
  MakeTypicalPeerConnectionCall("callWithDataOnly();");
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer: http://crbug.com/405951
#define MAYBE_CallWithSctpDataOnly DISABLED_CallWithSctpDataOnly
#else
#define MAYBE_CallWithSctpDataOnly CallWithSctpDataOnly
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, MAYBE_CallWithSctpDataOnly) {
  MakeTypicalPeerConnectionCall("callWithSctpDataOnly();");
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
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, DISABLED_CallWithDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithDataAndMedia();");
}


#if (defined(OS_LINUX) && !defined(OS_CHROMEOS) && \
     defined(ARCH_CPU_ARM_FAMILY)) || defined(MEMORY_SANITIZER)
// Timing out on ARM linux bot: http://crbug.com/238490
// Fails under MemorySanitizer: http://crbug.com/405951
#define MAYBE_CallWithSctpDataAndMedia DISABLED_CallWithSctpDataAndMedia
#else
#define MAYBE_CallWithSctpDataAndMedia CallWithSctpDataAndMedia
#endif

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CallWithSctpDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithSctpDataAndMedia();");
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
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CallWithDataAndLaterAddMedia) {
  MakeTypicalPeerConnectionCall("callWithDataAndLaterAddMedia();");
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
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CallWithNewVideoMediaStream) {
  MakeTypicalPeerConnectionCall("callWithNewVideoMediaStream();");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia. When video is flowing, the VideoTrack is removed and an
// AudioTrack is added instead.
// TODO(phoglund): This test is manual since not all buildbots has an audio
// input.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, MANUAL_CallAndModifyStream) {
  MakeTypicalPeerConnectionCall(
      "callWithNewVideoMediaStreamLaterSwitchToAudio();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, AddTwoMediaStreamsToOnePC) {
  MakeTypicalPeerConnectionCall("addTwoMediaStreamsToOneConnection();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioVideoCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureAudioIsPlaying(%s, {audio:true, video:true});",
      kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioOnlyCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureAudioIsPlaying(%s, {audio:true});",
      kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioVideoCallAndVerifyRemoteMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureRemoteAudioTrackMutingWorks(%s);",
      kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioVideoCallAndVerifyLocalMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureLocalAudioTrackMutingWorks(%s);",
      kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EnsureLocalVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureLocalVideoMutingDoesntMuteAudio(%s);",
      kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EnsureRemoteVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureRemoteVideoMutingDoesntMuteAudio(%s);",
      kUseLenientAudioChecking));
}

// Flaky on TSAN v2: http://crbug.com/373637
#if defined(THREAD_SANITIZER)
#define MAYBE_EstablishAudioVideoCallAndVerifyUnmutingWorks\
        DISABLED_EstablishAudioVideoCallAndVerifyUnmutingWorks
#else
#define MAYBE_EstablishAudioVideoCallAndVerifyUnmutingWorks\
        EstablishAudioVideoCallAndVerifyUnmutingWorks
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_EstablishAudioVideoCallAndVerifyUnmutingWorks) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureAudioTrackUnmutingWorks(%s);", kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallAndVerifyVideoMutingWorks) {
  MakeTypicalPeerConnectionCall("callAndEnsureVideoTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CreateOfferWithOfferOptions) {
  MakeTypicalPeerConnectionCall("testCreateOfferOptions();");
}

}  // namespace content
