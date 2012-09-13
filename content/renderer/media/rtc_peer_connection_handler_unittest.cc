// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaConstraints.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCConfiguration.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCICECandidate.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCSessionDescription.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCVoidRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"

static const char kDummySdp[] = "dummy sdp";
static const char kDummySdpType[] = "dummy type";

class RTCPeerConnectionHandlerUnderTest : public RTCPeerConnectionHandler {
 public:
  RTCPeerConnectionHandlerUnderTest(
      WebKit::WebRTCPeerConnectionHandlerClient* client,
      MediaStreamDependencyFactory* dependency_factory)
      : RTCPeerConnectionHandler(client, dependency_factory) {
  }

  webrtc::MockPeerConnectionImpl* native_peer_connection() {
    return static_cast<webrtc::MockPeerConnectionImpl*>(
        native_peer_connection_.get());
  }
};

class RTCPeerConnectionHandlerTest : public ::testing::Test {
 public:
  RTCPeerConnectionHandlerTest() : mock_peer_connection_(NULL) {
  }

  void SetUp() {
    mock_client_.reset(new WebKit::MockWebRTCPeerConnectionHandlerClient());
    mock_dependency_factory_.reset(new MockMediaStreamDependencyFactory());
    mock_dependency_factory_->EnsurePeerConnectionFactory();
    pc_handler_.reset(
        new RTCPeerConnectionHandlerUnderTest(mock_client_.get(),
                                              mock_dependency_factory_.get()));

    WebKit::WebRTCConfiguration config;
    WebKit::WebMediaConstraints constraints;
    EXPECT_TRUE(pc_handler_->initialize(config, constraints));

    mock_peer_connection_ = pc_handler_->native_peer_connection();
    ASSERT_TRUE(mock_peer_connection_);
  }

  void Initialize(const std::string& server, const std::string& password) {
    WebKit::WebRTCConfiguration config;
    WebKit::WebMediaConstraints constraints;

    // TODO(perkj): Test that the parameters in |config| can be translated when
    // a WebRTCConfiguration can be constructed. It's WebKit class and can't be
    // initialized from a test.
    EXPECT_TRUE(pc_handler_->initialize(config, constraints));

    mock_peer_connection_ = pc_handler_->native_peer_connection();
    ASSERT_TRUE(mock_peer_connection_);
  }

  // Creates a WebKit local MediaStream.
  WebKit::WebMediaStreamDescriptor CreateLocalMediaStream(
      const std::string& stream_label) {
    std::string video_track_label("video-label");
    std::string audio_track_label("audio-label");

    scoped_refptr<webrtc::LocalMediaStreamInterface> native_stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label));
    scoped_refptr<webrtc::LocalAudioTrackInterface> audio_track(
        mock_dependency_factory_->CreateLocalAudioTrack(audio_track_label,
                                                        NULL));
    native_stream->AddTrack(audio_track);
    scoped_refptr<webrtc::LocalVideoTrackInterface> video_track(
        mock_dependency_factory_->CreateLocalVideoTrack(video_track_label, 0));
    native_stream->AddTrack(video_track);

    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
        static_cast<size_t>(1));
    audio_sources[0].initialize(WebKit::WebString::fromUTF8(video_track_label),
                                WebKit::WebMediaStreamSource::TypeAudio,
                                WebKit::WebString::fromUTF8("audio_track"));
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        static_cast<size_t>(1));
    video_sources[0].initialize(WebKit::WebString::fromUTF8(video_track_label),
                                WebKit::WebMediaStreamSource::TypeVideo,
                                WebKit::WebString::fromUTF8("video_track"));
    WebKit::WebMediaStreamDescriptor local_stream;
    local_stream.initialize(UTF8ToUTF16(stream_label), audio_sources,
                            video_sources);
    local_stream.setExtraData(new MediaStreamExtraData(native_stream));
    return local_stream;
  }

  // Creates a remote MediaStream and adds it to the mocked native
  // peer connection.
  scoped_refptr<webrtc::MediaStreamInterface>
  AddRemoteMockMediaStream(const std::string& stream_label,
                           const std::string& video_track_label,
                           const std::string& audio_track_label) {
    // We use a local stream as a remote since for testing purposes we really
    // only need the MediaStreamInterface.
    scoped_refptr<webrtc::LocalMediaStreamInterface> stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label));
    if (!video_track_label.empty()) {
      scoped_refptr<webrtc::LocalVideoTrackInterface> video_track(
          mock_dependency_factory_->CreateLocalVideoTrack(video_track_label,
                                                          0));
      stream->AddTrack(video_track);
    }
    if (!audio_track_label.empty()) {
      scoped_refptr<webrtc::LocalAudioTrackInterface> audio_track(
          mock_dependency_factory_->CreateLocalAudioTrack(audio_track_label,
                                                          NULL));
      stream->AddTrack(audio_track);
    }
    mock_peer_connection_->AddRemoteStream(stream);
    return stream;
  }

  scoped_ptr<WebKit::MockWebRTCPeerConnectionHandlerClient> mock_client_;
  scoped_ptr<MockMediaStreamDependencyFactory> mock_dependency_factory_;
  scoped_ptr<RTCPeerConnectionHandlerUnderTest> pc_handler_;

  // Weak reference to the mocked native peer connection implementation.
  webrtc::MockPeerConnectionImpl* mock_peer_connection_;
};

TEST_F(RTCPeerConnectionHandlerTest, Initialize) {
  Initialize("dummy", "dummy_pwd");
}

TEST_F(RTCPeerConnectionHandlerTest, CreateOffer) {
  WebKit::WebRTCSessionDescriptionRequest request;
  WebKit::WebMediaConstraints options;
  // TODO(perkj): Can WebKit::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != NULL);
  pc_handler_->createOffer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateAnser) {
  WebKit::WebRTCSessionDescriptionRequest request;
  WebKit::WebMediaConstraints options;
  // TODO(perkj): Can WebKit::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != NULL);
  pc_handler_->createAnswer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, setLocalDescription) {
  WebKit::WebRTCVoidRequest request;
  WebKit::WebRTCSessionDescription description;
  description.initialize(kDummySdpType, kDummySdp);
  pc_handler_->setLocalDescription(request, description);
  EXPECT_EQ(description.type(), pc_handler_->localDescription().type());
  EXPECT_EQ(description.sdp(), pc_handler_->localDescription().sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->local_description() != NULL);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->local_description()->type());
  mock_peer_connection_->local_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);
}

TEST_F(RTCPeerConnectionHandlerTest, setRemoteDescription) {
  WebKit::WebRTCVoidRequest request;
  WebKit::WebRTCSessionDescription description;
  description.initialize(kDummySdpType, kDummySdp);
  pc_handler_->setRemoteDescription(request, description);
  EXPECT_EQ(description.type(), pc_handler_->remoteDescription().type());
  EXPECT_EQ(description.sdp(), pc_handler_->remoteDescription().sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->remote_description() != NULL);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->remote_description()->type());
  mock_peer_connection_->remote_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);
}

TEST_F(RTCPeerConnectionHandlerTest, updateICE) {
  WebKit::WebRTCConfiguration config;
  WebKit::WebMediaConstraints constraints;

  // TODO(perkj): Test that the parameters in |config| can be translated when a
  // WebRTCConfiguration can be constructed. It's WebKit class and can't be
  // initialized from a test.
  EXPECT_TRUE(pc_handler_->updateICE(config, constraints));
}

TEST_F(RTCPeerConnectionHandlerTest, addICECandidate) {
  WebKit::WebRTCICECandidate candidate;
  candidate.initialize(kDummySdp, "mid", 1);
  EXPECT_TRUE(pc_handler_->addICECandidate(candidate));
  EXPECT_EQ(kDummySdp, mock_peer_connection_->ice_sdp());
  EXPECT_EQ(1, mock_peer_connection_->sdp_mline_index());
  EXPECT_EQ("mid", mock_peer_connection_->sdp_mid());
}

TEST_F(RTCPeerConnectionHandlerTest, addAndRemoveStream) {
  std::string stream_label = "local_stream";
  WebKit::WebMediaStreamDescriptor local_stream(
      CreateLocalMediaStream(stream_label));
  WebKit::WebMediaConstraints constraints;

  EXPECT_TRUE(pc_handler_->addStream(local_stream, constraints));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->audio_tracks()->count());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->video_tracks()->count());

  pc_handler_->removeStream(local_stream);
  EXPECT_EQ(0u, mock_peer_connection_->local_streams()->count());
}

TEST_F(RTCPeerConnectionHandlerTest, OnStateChange) {
  // Ready states.
  webrtc::PeerConnectionObserver::StateType state =
      webrtc::PeerConnectionObserver::kReadyState;
  mock_peer_connection_->SetReadyState(
      webrtc::PeerConnectionInterface::kOpening);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateOpening,
            mock_client_->ready_state());
  mock_peer_connection_->SetReadyState(
      webrtc::PeerConnectionInterface::kActive);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateActive,
            mock_client_->ready_state());
  mock_peer_connection_->SetReadyState(
      webrtc::PeerConnectionInterface::kClosing);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateClosing,
            mock_client_->ready_state());
  mock_peer_connection_->SetReadyState(
        webrtc::PeerConnectionInterface::kClosed);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateClosed,
            mock_client_->ready_state());

  // Ice states.
  state = webrtc::PeerConnectionObserver::kIceState;
  mock_peer_connection_->SetIceState(
      webrtc::PeerConnectionInterface::kIceGathering);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ICEStateGathering,
            mock_client_->ice_state());
  mock_peer_connection_->SetIceState(
      webrtc::PeerConnectionInterface::kIceWaiting);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ICEStateWaiting,
            mock_client_->ice_state());
  mock_peer_connection_->SetIceState(
      webrtc::PeerConnectionInterface::kIceChecking);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ICEStateChecking,
            mock_client_->ice_state());
  mock_peer_connection_->SetIceState(
      webrtc::PeerConnectionInterface::kIceConnected);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ICEStateConnected,
            mock_client_->ice_state());
  mock_peer_connection_->SetIceState(
      webrtc::PeerConnectionInterface::kIceCompleted);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ICEStateCompleted,
            mock_client_->ice_state());
  mock_peer_connection_->SetIceState(
      webrtc::PeerConnectionInterface::kIceFailed);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ICEStateFailed,
            mock_client_->ice_state());
  mock_peer_connection_->SetIceState(
      webrtc::PeerConnectionInterface::kIceClosed);
  pc_handler_->OnStateChange(state);
  EXPECT_EQ(WebKit::WebRTCPeerConnectionHandlerClient::ICEStateClosed,
            mock_client_->ice_state());
}

TEST_F(RTCPeerConnectionHandlerTest, OnAddAndOnRemoveStream) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));
  pc_handler_->OnAddStream(remote_stream);
  EXPECT_EQ(remote_stream_label, mock_client_->stream_label());

  pc_handler_->OnRemoveStream(remote_stream);
  EXPECT_TRUE(mock_client_->stream_label().empty());
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceCandidateAndOnIceComplete) {
  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory_->CreateIceCandidate("mid", 1, kDummySdp));
  pc_handler_->OnIceCandidate(native_candidate.get());
  EXPECT_EQ("mid", mock_client_->candidate_mid());
  EXPECT_EQ(1, mock_client_->candidate_mlineindex());
  EXPECT_EQ(kDummySdp, mock_client_->candidate_sdp());

  pc_handler_->OnIceComplete();
  EXPECT_EQ("", mock_client_->candidate_mid());
  EXPECT_EQ(-1, mock_client_->candidate_mlineindex());
  EXPECT_EQ("", mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnRenegotiationNeeded) {
  EXPECT_FALSE(mock_client_->renegotiate());
  pc_handler_->OnRenegotiationNeeded();
  EXPECT_TRUE(mock_client_->renegotiate());
}
