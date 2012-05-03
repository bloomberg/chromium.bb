// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_impl.h"
#include "content/renderer/media/mock_web_peer_connection_00_handler_client.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/peer_connection_handler_jsep.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "jingle/glue/thread_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICECandidateDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICEOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaHints.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSessionDescriptionDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace webrtc {

class MockVideoRendererWrapper : public VideoRendererWrapperInterface {
 public:
  virtual cricket::VideoRenderer* renderer() OVERRIDE { return NULL; }

 protected:
  virtual ~MockVideoRendererWrapper() {}
};

}  // namespace webrtc

TEST(PeerConnectionHandlerJsepTest, Basic) {
  MessageLoop loop;

  scoped_ptr<WebKit::MockWebPeerConnection00HandlerClient> mock_client(
      new WebKit::MockWebPeerConnection00HandlerClient());
  scoped_ptr<MockMediaStreamImpl> mock_ms_impl(new MockMediaStreamImpl());
  scoped_ptr<MockMediaStreamDependencyFactory> mock_dependency_factory(
      new MockMediaStreamDependencyFactory(NULL));
  mock_dependency_factory->CreatePeerConnectionFactory(NULL,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       NULL);
  scoped_ptr<PeerConnectionHandlerJsep> pc_handler(
      new PeerConnectionHandlerJsep(mock_client.get(),
                                    mock_ms_impl.get(),
                                    mock_dependency_factory.get()));

  WebKit::WebString server_config(
      WebKit::WebString::fromUTF8("STUN stun.l.google.com:19302"));
  WebKit::WebString username;
  pc_handler->initialize(server_config, username);
  EXPECT_TRUE(pc_handler->native_peer_connection_.get());
  webrtc::MockPeerConnectionImpl* mock_peer_connection =
      static_cast<webrtc::MockPeerConnectionImpl*>(
          pc_handler->native_peer_connection_.get());

  // Create offer.
  WebKit::WebMediaHints hints;
  hints.initialize(true, true);
  WebKit::WebSessionDescriptionDescriptor offer =
      pc_handler->createOffer(hints);
  EXPECT_FALSE(offer.isNull());
  EXPECT_EQ(std::string(mock_peer_connection->kDummyOffer),
            UTF16ToUTF8(offer.initialSDP()));
  EXPECT_EQ(hints.audio(), mock_peer_connection->hint_audio());
  EXPECT_EQ(hints.video(), mock_peer_connection->hint_video());

  // Create answer.
  WebKit::WebString offer_string = "offer";
  hints.reset();
  hints.initialize(false, false);
  WebKit::WebSessionDescriptionDescriptor answer =
      pc_handler->createAnswer(offer_string, hints);
  EXPECT_FALSE(answer.isNull());
  EXPECT_EQ(UTF16ToUTF8(offer_string), UTF16ToUTF8(answer.initialSDP()));
  EXPECT_EQ(UTF16ToUTF8(offer_string), mock_peer_connection->description_sdp());
  EXPECT_EQ(hints.audio(), mock_peer_connection->hint_audio());
  EXPECT_EQ(hints.video(), mock_peer_connection->hint_video());

  // Set local description.
  PeerConnectionHandlerJsep::Action action =
      PeerConnectionHandlerJsep::ActionSDPOffer;
  WebKit::WebSessionDescriptionDescriptor description;
  WebKit::WebString sdp = "test sdp";
  description.initialize(sdp);
  EXPECT_TRUE(pc_handler->setLocalDescription(action, description));
  EXPECT_EQ(webrtc::PeerConnectionInterface::kOffer,
            mock_peer_connection->action());
  EXPECT_EQ(UTF16ToUTF8(sdp), mock_peer_connection->description_sdp());

  // Get local description.
  description.reset();
  description = pc_handler->localDescription();
  EXPECT_FALSE(description.isNull());
  EXPECT_EQ(UTF16ToUTF8(sdp), UTF16ToUTF8(description.initialSDP()));

  // Set remote description.
  action = PeerConnectionHandlerJsep::ActionSDPAnswer;
  sdp = "test sdp 2";
  description.reset();
  description.initialize(sdp);
  EXPECT_TRUE(pc_handler->setRemoteDescription(action, description));
  EXPECT_EQ(webrtc::PeerConnectionInterface::kAnswer,
            mock_peer_connection->action());
  EXPECT_EQ(UTF16ToUTF8(sdp), mock_peer_connection->description_sdp());

  // Get remote description.
  description.reset();
  description = pc_handler->remoteDescription();
  EXPECT_FALSE(description.isNull());
  EXPECT_EQ(UTF16ToUTF8(sdp), UTF16ToUTF8(description.initialSDP()));

  // Start ICE.
  WebKit::WebICEOptions options;
  options.initialize(WebKit::WebICEOptions::CandidateTypeAll);
  EXPECT_TRUE(pc_handler->startIce(options));
  EXPECT_EQ(webrtc::PeerConnectionInterface::kUseAll,
            mock_peer_connection->ice_options());

  // Process ICE message.
  WebKit::WebICECandidateDescriptor candidate;
  WebKit::WebString label = "test label";
  sdp = "test sdp";
  candidate.initialize(label, sdp);
  EXPECT_TRUE(pc_handler->processIceMessage(candidate));
  EXPECT_EQ(UTF16ToUTF8(label), mock_peer_connection->ice_label());
  EXPECT_EQ(UTF16ToUTF8(sdp), mock_peer_connection->ice_sdp());

  // Add stream.
  // TODO(grunell): Add an audio track as well.
  std::string stream_label("stream-label");
  std::string video_track_label("video-label");

  talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> native_stream(
      mock_dependency_factory->CreateLocalMediaStream(stream_label));
  talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> local_video_track(
      mock_dependency_factory->CreateLocalVideoTrack(video_track_label, 0));
  native_stream->AddTrack(local_video_track);
  mock_ms_impl->AddLocalStream(native_stream);
  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector(
      static_cast<size_t>(1));
  source_vector[0].initialize(WebKit::WebString::fromUTF8(video_track_label),
                              WebKit::WebMediaStreamSource::TypeVideo,
                              WebKit::WebString::fromUTF8("RemoteVideo"));
  WebKit::WebMediaStreamDescriptor local_stream;
  local_stream.initialize(UTF8ToUTF16(stream_label), source_vector);
  pc_handler->addStream(local_stream);
  EXPECT_EQ(stream_label, mock_peer_connection->stream_label());
  EXPECT_TRUE(mock_peer_connection->stream_changes_committed());

  // On add stream.
  std::string remote_stream_label(stream_label);
  remote_stream_label += "-remote";
  std::string remote_video_track_label(video_track_label);
  remote_video_track_label += "-remote";
  // We use a local stream as a remote since for testing purposes we really
  // only need the MediaStreamInterface.
  talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> remote_stream(
      mock_dependency_factory->CreateLocalMediaStream(remote_stream_label));
  talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> remote_video_track(
      mock_dependency_factory->CreateLocalVideoTrack(remote_video_track_label,
                                                     0));
  remote_video_track->set_enabled(true);
  remote_stream->AddTrack(remote_video_track);
  mock_peer_connection->AddRemoteStream(remote_stream);
  pc_handler->OnAddStream(remote_stream);
  EXPECT_EQ(remote_stream_label, mock_client->stream_label());

  // Set renderer.
  talk_base::scoped_refptr<webrtc::MockVideoRendererWrapper> renderer(
      new talk_base::RefCountedObject<webrtc::MockVideoRendererWrapper>());
  pc_handler->SetRemoteVideoRenderer(remote_video_track_label, renderer);
  EXPECT_EQ(renderer, static_cast<webrtc::MockLocalVideoTrack*>(
      remote_video_track.get())->renderer());

  // Remove stream.
  WebKit::WebVector<WebKit::WebMediaStreamDescriptor> empty_streams(
      static_cast<size_t>(0));
  pc_handler->removeStream(local_stream);
  EXPECT_EQ("", mock_peer_connection->stream_label());
  mock_peer_connection->ClearStreamChangesCommitted();
  EXPECT_TRUE(!mock_peer_connection->stream_changes_committed());

  // On remove stream.
  pc_handler->OnRemoveStream(remote_stream);
  EXPECT_TRUE(mock_client->stream_label().empty());

  // Add stream again.
  pc_handler->addStream(local_stream);
  EXPECT_EQ(stream_label, mock_peer_connection->stream_label());
  EXPECT_TRUE(mock_peer_connection->stream_changes_committed());

  // On state change.
  mock_peer_connection->SetReadyState(webrtc::PeerConnectionInterface::kActive);
  webrtc::PeerConnectionObserver::StateType state =
      webrtc::PeerConnectionObserver::kReadyState;
  pc_handler->OnStateChange(state);
  EXPECT_EQ(WebKit::WebPeerConnection00HandlerClient::ReadyStateActive,
            mock_client->ready_state());

  // On ICE candidate.
  std::string candidate_label = "test label";
  std::string candidate_sdp = "test sdp";
  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory->CreateIceCandidate(candidate_label,
                                                  candidate_sdp));
  pc_handler->OnIceCandidate(native_candidate.get());
  EXPECT_EQ(candidate_label, mock_client->candidate_label());
  EXPECT_EQ(candidate_sdp, mock_client->candidate_sdp());
  EXPECT_TRUE(mock_client->more_to_follow());

  // On ICE complete.
  pc_handler->OnIceComplete();
  EXPECT_TRUE(mock_client->candidate_label().empty());
  EXPECT_TRUE(mock_client->candidate_sdp().empty());
  EXPECT_FALSE(mock_client->more_to_follow());

  // Stop.
  pc_handler->stop();
  EXPECT_FALSE(pc_handler->native_peer_connection_.get());

  // PC handler is expected to be deleted when stop calls
  // MediaStreamImpl::ClosePeerConnection. We own and delete it here instead of
  // in the mock.
  pc_handler.reset();
}
