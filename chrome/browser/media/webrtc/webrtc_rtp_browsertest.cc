// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"

static const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

class WebRtcRtpBrowserTest : public WebRtcTestBase {
 public:
  WebRtcRtpBrowserTest() : left_tab_(nullptr), right_tab_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "RTCRtpSender");
    // Required by |CollectGarbage|.
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }

 protected:
  void StartServerAndOpenTabs() {
    ASSERT_TRUE(embedded_test_server()->Start());
    left_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
    right_tab_ = OpenTestPageAndGetUserMediaInNewTab(kMainWebrtcTestHtmlPage);
  }

  content::WebContents* left_tab_;
  content::WebContents* right_tab_;
};

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest, GetSenders) {
  StartServerAndOpenTabs();

  SetupPeerconnectionWithoutLocalStream(left_tab_);
  CreateAndAddStreams(left_tab_, 3);

  SetupPeerconnectionWithoutLocalStream(right_tab_);
  CreateAndAddStreams(right_tab_, 1);

  NegotiateCall(left_tab_, right_tab_);

  VerifyRtpSenders(left_tab_, 6);
  VerifyRtpSenders(right_tab_, 2);
}

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest, GetReceivers) {
  StartServerAndOpenTabs();

  SetupPeerconnectionWithoutLocalStream(left_tab_);
  CreateAndAddStreams(left_tab_, 3);

  SetupPeerconnectionWithoutLocalStream(right_tab_);
  CreateAndAddStreams(right_tab_, 1);

  NegotiateCall(left_tab_, right_tab_);

  VerifyRtpReceivers(left_tab_, 2);
  VerifyRtpReceivers(right_tab_, 6);
}

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest, AddAndRemoveTracksWithoutStream) {
  StartServerAndOpenTabs();

  SetupPeerconnectionWithoutLocalStream(left_tab_);
  SetupPeerconnectionWithoutLocalStream(right_tab_);

  // TODO(hbos): Here and in other "AddAndRemoveTracks" tests: when ontrack and
  // ended events are supported, verify that these are fired on the remote side
  // when tracks are added and removed. https://crbug.com/webrtc/7933

  // Add two tracks.
  EXPECT_EQ(0u, GetNegotiationNeededCount(left_tab_));
  std::vector<std::string> ids =
      CreateAndAddAudioAndVideoTrack(left_tab_, StreamArgumentType::NO_STREAM);
  // TODO(hbos): Should only fire once (if the "negotiationneeded" bit changes
  // from false to true), not once per track added. https://crbug.com/740501
  EXPECT_EQ(2u, GetNegotiationNeededCount(left_tab_));
  std::string audio_stream_id = ids[0];
  std::string audio_track_id = ids[1];
  std::string video_stream_id = ids[2];
  std::string video_track_id = ids[3];
  EXPECT_EQ("null", audio_stream_id);
  EXPECT_NE("null", audio_track_id);
  EXPECT_EQ("null", video_stream_id);
  EXPECT_NE("null", video_track_id);
  CollectGarbage(left_tab_);
  EXPECT_FALSE(
      HasLocalStreamWithTrack(left_tab_, audio_stream_id, audio_track_id));
  EXPECT_FALSE(
      HasLocalStreamWithTrack(left_tab_, video_stream_id, video_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 2);
  // Negotiate call, sets remote description.
  NegotiateCall(left_tab_, right_tab_);
  // TODO(hbos): When |addTrack| without streams results in SDP that does not
  // signal a remote stream to be added this should be EXPECT_FALSE.
  // https://crbug.com/webrtc/7933
  EXPECT_TRUE(HasRemoteStreamWithTrack(right_tab_, kUndefined, audio_track_id));
  EXPECT_TRUE(HasRemoteStreamWithTrack(right_tab_, kUndefined, video_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 2);

  // Remove first track.
  RemoveTrack(left_tab_, audio_track_id);
  CollectGarbage(left_tab_);
  EXPECT_EQ(3u, GetNegotiationNeededCount(left_tab_));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 1);
  // Re-negotiate call, sets remote description again.
  NegotiateCall(left_tab_, right_tab_);
  CollectGarbage(right_tab_);
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, kUndefined, audio_track_id));
  EXPECT_TRUE(HasRemoteStreamWithTrack(right_tab_, kUndefined, video_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 1);

  // Remove second track.
  RemoveTrack(left_tab_, video_track_id);
  CollectGarbage(left_tab_);
  EXPECT_EQ(4u, GetNegotiationNeededCount(left_tab_));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 0);
  // Re-negotiate call, sets remote description again.
  NegotiateCall(left_tab_, right_tab_);
  CollectGarbage(right_tab_);
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, kUndefined, audio_track_id));
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, kUndefined, video_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 0);
}

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest,
                       AddAndRemoveTracksWithSharedStream) {
  StartServerAndOpenTabs();

  SetupPeerconnectionWithoutLocalStream(left_tab_);
  SetupPeerconnectionWithoutLocalStream(right_tab_);

  // Add two tracks.
  EXPECT_EQ(0u, GetNegotiationNeededCount(left_tab_));
  std::vector<std::string> ids = CreateAndAddAudioAndVideoTrack(
      left_tab_, StreamArgumentType::SHARED_STREAM);
  // TODO(hbos): Should only fire once (if the "negotiationneeded" bit changes
  // from false to true), not once per track added. https://crbug.com/740501
  EXPECT_EQ(2u, GetNegotiationNeededCount(left_tab_));
  std::string audio_stream_id = ids[0];
  std::string audio_track_id = ids[1];
  std::string video_stream_id = ids[2];
  std::string video_track_id = ids[3];
  EXPECT_NE("null", audio_stream_id);
  EXPECT_NE("null", audio_track_id);
  EXPECT_NE("null", video_stream_id);
  EXPECT_NE("null", video_track_id);
  EXPECT_EQ(audio_stream_id, video_stream_id);
  CollectGarbage(left_tab_);
  // TODO(hbos): When |getLocalStreams| is updated to return the streams of all
  // senders, not just |addStream|-streams, then this will be EXPECT_TRUE.
  // https://crbug.com/738918
  EXPECT_FALSE(
      HasLocalStreamWithTrack(left_tab_, audio_stream_id, audio_track_id));
  EXPECT_FALSE(
      HasLocalStreamWithTrack(left_tab_, video_stream_id, video_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 2);
  // Negotiate call, sets remote description.
  NegotiateCall(left_tab_, right_tab_);
  EXPECT_TRUE(
      HasRemoteStreamWithTrack(right_tab_, audio_stream_id, audio_track_id));
  EXPECT_TRUE(
      HasRemoteStreamWithTrack(right_tab_, video_stream_id, video_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 2);

  // Remove first track.
  RemoveTrack(left_tab_, audio_track_id);
  CollectGarbage(left_tab_);
  EXPECT_EQ(3u, GetNegotiationNeededCount(left_tab_));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 1);
  // Re-negotiate call, sets remote description again.
  NegotiateCall(left_tab_, right_tab_);
  CollectGarbage(right_tab_);
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, audio_stream_id, audio_track_id));
  EXPECT_TRUE(
      HasRemoteStreamWithTrack(right_tab_, video_stream_id, video_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 1);

  // Remove second track.
  RemoveTrack(left_tab_, video_track_id);
  CollectGarbage(left_tab_);
  EXPECT_EQ(4u, GetNegotiationNeededCount(left_tab_));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 0);
  // Re-negotiate call, sets remote description again.
  NegotiateCall(left_tab_, right_tab_);
  CollectGarbage(right_tab_);
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, audio_stream_id, audio_track_id));
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, video_stream_id, video_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 0);
}

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest,
                       AddAndRemoveTracksWithIndividualStreams) {
  StartServerAndOpenTabs();

  SetupPeerconnectionWithoutLocalStream(left_tab_);
  SetupPeerconnectionWithoutLocalStream(right_tab_);

  // Add two tracks.
  EXPECT_EQ(0u, GetNegotiationNeededCount(left_tab_));
  std::vector<std::string> ids = CreateAndAddAudioAndVideoTrack(
      left_tab_, StreamArgumentType::INDIVIDUAL_STREAMS);
  // TODO(hbos): Should only fire once (if the "negotiationneeded" bit changes
  // from false to true), not once per track added. https://crbug.com/740501
  EXPECT_EQ(2u, GetNegotiationNeededCount(left_tab_));
  std::string audio_stream_id = ids[0];
  std::string audio_track_id = ids[1];
  std::string video_stream_id = ids[2];
  std::string video_track_id = ids[3];
  EXPECT_NE("null", audio_stream_id);
  EXPECT_NE("null", audio_track_id);
  EXPECT_NE("null", video_stream_id);
  EXPECT_NE("null", video_track_id);
  EXPECT_NE(audio_stream_id, video_stream_id);
  CollectGarbage(left_tab_);
  // TODO(hbos): When |getLocalStreams| is updated to return the streams of all
  // senders, not just |addStream|-streams, then this will be EXPECT_TRUE.
  // https://crbug.com/738918
  EXPECT_FALSE(
      HasLocalStreamWithTrack(left_tab_, audio_stream_id, audio_track_id));
  EXPECT_FALSE(
      HasLocalStreamWithTrack(left_tab_, video_stream_id, video_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 2);
  // Negotiate call, sets remote description.
  NegotiateCall(left_tab_, right_tab_);
  EXPECT_TRUE(
      HasRemoteStreamWithTrack(right_tab_, audio_stream_id, audio_track_id));
  EXPECT_TRUE(
      HasRemoteStreamWithTrack(right_tab_, video_stream_id, video_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 2);

  // Remove first track.
  RemoveTrack(left_tab_, audio_track_id);
  CollectGarbage(left_tab_);
  EXPECT_EQ(3u, GetNegotiationNeededCount(left_tab_));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_TRUE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 1);
  // Re-negotiate call, sets remote description again.
  NegotiateCall(left_tab_, right_tab_);
  CollectGarbage(right_tab_);
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, audio_stream_id, audio_track_id));
  EXPECT_TRUE(
      HasRemoteStreamWithTrack(right_tab_, video_stream_id, video_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_TRUE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 1);

  // Remove second track.
  RemoveTrack(left_tab_, video_track_id);
  CollectGarbage(left_tab_);
  EXPECT_EQ(4u, GetNegotiationNeededCount(left_tab_));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, audio_track_id));
  EXPECT_FALSE(HasSenderWithTrack(left_tab_, video_track_id));
  VerifyRtpSenders(left_tab_, 0);
  // Re-negotiate call, sets remote description again.
  NegotiateCall(left_tab_, right_tab_);
  CollectGarbage(right_tab_);
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, audio_stream_id, audio_track_id));
  EXPECT_FALSE(
      HasRemoteStreamWithTrack(right_tab_, video_stream_id, video_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, audio_track_id));
  EXPECT_FALSE(HasReceiverWithTrack(right_tab_, video_track_id));
  VerifyRtpReceivers(right_tab_, 0);
}

IN_PROC_BROWSER_TEST_F(WebRtcRtpBrowserTest, GetReceiversSetRemoteDescription) {
  StartServerAndOpenTabs();
  EXPECT_EQ("ok", ExecuteJavascript("createReceiverWithSetRemoteDescription()",
                                    left_tab_));
}
