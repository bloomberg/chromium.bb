// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

#if defined (OS_ANDROID) || defined(THREAD_SANITIZER)
// Just do the bare minimum of audio checking on Android and under TSAN since
// it's a bit sensitive to device performance.
const char kUseLenientAudioChecking[] = "true";
#else
const char kUseLenientAudioChecking[] = "false";
#endif

const int kExpectedConsumerId = 0;

// Get the ID for the render process host when there should only be one.
bool GetRenderProcessHostId(base::ProcessId* id) {
  content::RenderProcessHost::iterator it(
      content::RenderProcessHost::AllHostsIterator());
  *id = base::GetProcId(it.GetCurrentValue()->GetHandle());
  EXPECT_NE(base::kNullProcessId, *id);
  if (*id == base::kNullProcessId)
    return false;
  it.Advance();
  EXPECT_TRUE(it.IsAtEnd());
  return it.IsAtEnd();
}

}  // namespace

namespace content {

class WebRtcBrowserTest : public WebRtcContentBrowserTest {
 public:
  WebRtcBrowserTest() {}
  virtual ~WebRtcBrowserTest() {}

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

    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream))
            << "Must run with fake devices since the test will explicitly look "
            << "for the fake device signal.";

    MakeTypicalPeerConnectionCall(javascript);
  }

  void DisableOpusIfOnAndroid() {
#if defined(OS_ANDROID)
    // Always force iSAC 16K on Android for now (Opus is broken).
    EXPECT_EQ("isac-forced",
              ExecuteJavascriptAndReturnResult("forceIsac16KInSdp();"));
#endif
  }
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CanSetupDefaultVideoCall DISABLED_CanSetupDefaultVideoCall
#else
#define MAYBE_CanSetupDefaultVideoCall CanSetupDefaultVideoCall
#endif

// These tests will make a complete PeerConnection-based call and verify that
// video is playing for the call.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CanSetupDefaultVideoCall) {
  MakeTypicalPeerConnectionCall(
      "callAndExpectResolution({video: true}, 640, 480);");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       CanSetupVideoCallWith1To1AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 320,"
      " maxWidth: 320, minHeight: 320, maxHeight: 320}}}, 320, 320);";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       CanSetupVideoCallWith16To9AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 640,"
      " maxWidth: 640, minAspectRatio: 1.777}}}, 640, 360);";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       CanSetupVideoCallWith4To3AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 960,"
      "maxAspectRatio: 1.333}}}, 960, 720);";
  MakeTypicalPeerConnectionCall(javascript);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240376
#define MAYBE_CanSetupAudioAndVideoCall DISABLED_CanSetupAudioAndVideoCall
#else
#define MAYBE_CanSetupAudioAndVideoCall CanSetupAudioAndVideoCall
#endif

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CanSetupAudioAndVideoCall) {
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MANUAL_CanSetupCallAndSendDtmf) {
  MakeTypicalPeerConnectionCall("callAndSendDtmf(\'123,abc\');");
}

// TODO(phoglund): this test fails because the peer connection state will be
// stable in the second negotiation round rather than have-local-offer.
// http://crbug.com/293125.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
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
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CanForwardRemoteStream) {
#if defined (OS_ANDROID)
  // This test fails on Nexus 5 devices.
  // TODO(henrika): see http://crbug.com/362437 and http://crbug.com/359389
  // for details.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebRtcHWDecoding);
#endif
  MakeTypicalPeerConnectionCall(
      "callAndForwardRemoteStream({video: true, audio: false});");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CanForwardRemoteStream720p) {
#if defined (OS_ANDROID)
  // This test fails on Nexus 5 devices.
  // TODO(henrika): see http://crbug.com/362437 and http://crbug.com/359389
  // for details.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebRtcHWDecoding);
#endif
  const std::string javascript = GenerateGetUserMediaCall(
      "callAndForwardRemoteStream", 1280, 1280, 720, 720, 10, 30);
  MakeTypicalPeerConnectionCall(javascript);
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
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle) {
  MakeTypicalPeerConnectionCall("callWithoutMsidAndBundle();");
}

// This test will modify the SDP offer to an unsupported codec, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, NegotiateUnsupportedVideoCodec) {
  MakeTypicalPeerConnectionCall("negotiateUnsupportedVideoCodec();");
}

// This test will modify the SDP offer to use no encryption, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, NegotiateNonCryptoCall) {
  MakeTypicalPeerConnectionCall("negotiateNonCryptoCall();");
}

// This test can negotiate an SDP offer that includes a b=AS:xx to control
// the bandwidth for audio and video
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, NegotiateOfferWithBLine) {
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

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CanSetupLegacyCall) {
  MakeTypicalPeerConnectionCall("callWithLegacySdp();");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, CallWithDataOnly) {
  MakeTypicalPeerConnectionCall("callWithDataOnly();");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, CallWithSctpDataOnly) {
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
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, DISABLED_CallWithDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithDataAndMedia();");
}


#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithSctpDataAndMedia DISABLED_CallWithSctpDataAndMedia
#else
#define MAYBE_CallWithSctpDataAndMedia CallWithSctpDataAndMedia
#endif

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
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
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CallWithDataAndLaterAddMedia) {
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
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CallWithNewVideoMediaStream) {
  MakeTypicalPeerConnectionCall("callWithNewVideoMediaStream();");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia. When video is flowing, the VideoTrack is removed and an
// AudioTrack is added instead.
// TODO(phoglund): This test is manual since not all buildbots has an audio
// input.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MANUAL_CallAndModifyStream) {
  MakeTypicalPeerConnectionCall(
      "callWithNewVideoMediaStreamLaterSwitchToAudio();");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, AddTwoMediaStreamsToOnePC) {
  MakeTypicalPeerConnectionCall("addTwoMediaStreamsToOneConnection();");
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       EstablishAudioVideoCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureAudioIsPlaying(%s, {audio:true, video:true});",
      kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       EstablishAudioOnlyCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureAudioIsPlaying(%s, {audio:true});",
      kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       EstablishAudioVideoCallAndVerifyMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureAudioTrackMutingWorks(%s);", kUseLenientAudioChecking));
}

// Flaky on TSAN v2: http://crbug.com/373637
#if defined(THREAD_SANITIZER)
#define MAYBE_EstablishAudioVideoCallAndVerifyUnmutingWorks\
        DISABLED_EstablishAudioVideoCallAndVerifyUnmutingWorks
#else
#define MAYBE_EstablishAudioVideoCallAndVerifyUnmutingWorks\
        EstablishAudioVideoCallAndVerifyUnmutingWorks
#endif
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       MAYBE_EstablishAudioVideoCallAndVerifyUnmutingWorks) {
  MakeAudioDetectingPeerConnectionCall(base::StringPrintf(
      "callAndEnsureAudioTrackUnmutingWorks(%s);", kUseLenientAudioChecking));
}

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, CallAndVerifyVideoMutingWorks) {
  MakeTypicalPeerConnectionCall("callAndEnsureVideoTrackMutingWorks();");
}

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAecDump DISABLED_CallWithAecDump
#else
#define MAYBE_CallWithAecDump CallWithAecDump
#endif

// This tests will make a complete PeerConnection-based call, verify that
// video is playing for the call, and verify that a non-empty AEC dump file
// exists. The AEC dump is enabled through webrtc-internals. The HTML and
// Javascript is bypassed since it would trigger a file picker dialog. Instead,
// the dialog callback FileSelected() is invoked directly. In fact, there's
// never a webrtc-internals page opened at all since that's not needed.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_CallWithAecDump) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));
  base::DeleteFile(dump_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  DisableOpusIfOnAndroid();
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");

  EXPECT_FALSE(base::PathExists(dump_file));

  // Add file extensions that we expect to be added. The dump name will be
  // <temporary path>.<render process id>.<consumer id>, for example
  // "/tmp/.com.google.Chrome.Z6UC3P.12345.0".
  base::ProcessId render_process_id = base::kNullProcessId;
  EXPECT_TRUE(GetRenderProcessHostId(&render_process_id));
  dump_file = dump_file.AddExtension(IntToStringType(render_process_id))
                       .AddExtension(IntToStringType(kExpectedConsumerId));

  EXPECT_TRUE(base::PathExists(dump_file));
  int64 file_size = 0;
  EXPECT_TRUE(base::GetFileSize(dump_file, &file_size));
  EXPECT_GT(file_size, 0);

  base::DeleteFile(dump_file, false);
}

// TODO(grunell): Add test for multiple dumps when re-use of
// MediaStreamAudioProcessor in AudioCapturer has been removed.

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAecDumpEnabledThenDisabled DISABLED_CallWithAecDumpEnabledThenDisabled
#else
#define MAYBE_CallWithAecDumpEnabledThenDisabled CallWithAecDumpEnabledThenDisabled
#endif

// As above, but enable and disable dump before starting a call. The file should
// be created, but should be empty.
IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest,
                       MAYBE_CallWithAecDumpEnabledThenDisabled) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));
  base::DeleteFile(dump_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab, then disabling it.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);
  WebRTCInternals::GetInstance()->DisableAecDump();

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  DisableOpusIfOnAndroid();
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");

  // Add file extensions that we expect to be added.
  base::ProcessId render_process_id = base::kNullProcessId;
  EXPECT_TRUE(GetRenderProcessHostId(&render_process_id));
  dump_file = dump_file.AddExtension(IntToStringType(render_process_id))
                       .AddExtension(IntToStringType(kExpectedConsumerId));

  EXPECT_FALSE(base::PathExists(dump_file));

  base::DeleteFile(dump_file, false);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_TwoCallsWithAecDump DISABLED_TwoCallsWithAecDump
#else
#define MAYBE_TwoCallsWithAecDump TwoCallsWithAecDump
#endif

IN_PROC_BROWSER_TEST_F(WebRtcBrowserTest, MAYBE_TwoCallsWithAecDump) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  // Create a second window.
  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell2, GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));
  base::DeleteFile(dump_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));

  NavigateToURL(shell(), url);
  NavigateToURL(shell2, url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell2->web_contents(),
      "call({video: true, audio: true});",
      &result));
  ASSERT_STREQ("OK", result.c_str());

  EXPECT_FALSE(base::PathExists(dump_file));

  RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  for (; !it.IsAtEnd(); it.Advance()) {
    base::ProcessId render_process_id =
        base::GetProcId(it.GetCurrentValue()->GetHandle());
    EXPECT_NE(base::kNullProcessId, render_process_id);

    // Add file extensions that we expect to be added.
    base::FilePath unique_dump_file =
        dump_file.AddExtension(IntToStringType(render_process_id))
                 .AddExtension(IntToStringType(kExpectedConsumerId));

    EXPECT_TRUE(base::PathExists(unique_dump_file));
    int64 file_size = 0;
    EXPECT_TRUE(base::GetFileSize(unique_dump_file, &file_size));
    EXPECT_GT(file_size, 0);

    base::DeleteFile(unique_dump_file, false);
  }
}

}  // namespace content
