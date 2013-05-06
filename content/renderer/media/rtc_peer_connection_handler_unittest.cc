// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaConstraints.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCConfiguration.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCICECandidate.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCSessionDescription.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCStatsRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCVoidRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

static const char kDummySdp[] = "dummy sdp";
static const char kDummySdpType[] = "dummy type";

using WebKit::WebRTCPeerConnectionHandlerClient;
using testing::NiceMock;
using testing::_;
using testing::Ref;

namespace content {

class MockRTCStatsResponse : public LocalRTCStatsResponse {
 public:
  MockRTCStatsResponse()
      : report_count_(0),
        statistic_count_(0) {
  }

  virtual size_t addReport(WebKit::WebString type,
                           WebKit::WebString id,
                           double timestamp) OVERRIDE {
    ++report_count_;
    return report_count_;
  }

  virtual void addStatistic(size_t report,
                            WebKit::WebString name, WebKit::WebString value)
      OVERRIDE {
    ++statistic_count_;
  }
  int report_count() const { return report_count_; }

 private:
  int report_count_;
  int statistic_count_;
};

// Mocked wrapper for WebKit::WebRTCStatsRequest
class MockRTCStatsRequest : public LocalRTCStatsRequest {
 public:
  MockRTCStatsRequest()
      : has_selector_(false),
        stream_(),
        request_succeeded_called_(false) {}

  virtual bool hasSelector() const OVERRIDE {
    return has_selector_;
  }
  virtual WebKit::WebMediaStream stream() const OVERRIDE {
    return stream_;
  }
  virtual WebKit::WebMediaStreamTrack component() const OVERRIDE {
    return component_;
  }
  virtual scoped_refptr<LocalRTCStatsResponse> createResponse() OVERRIDE {
    DCHECK(!response_);
    response_ = new talk_base::RefCountedObject<MockRTCStatsResponse>();
    return response_;
  }

  virtual void requestSucceeded(const LocalRTCStatsResponse* response)
      OVERRIDE {
    EXPECT_EQ(response, response_.get());
    request_succeeded_called_ = true;
  }

  // Function for setting whether or not a selector is available.
  void setSelector(const WebKit::WebMediaStream& stream,
                   const WebKit::WebMediaStreamTrack& component) {
    has_selector_ = true;
    stream_ = stream;
    component_ = component;
  }

  // Function for inspecting the result of a stats request.
  MockRTCStatsResponse* result() {
    if (request_succeeded_called_) {
      return response_.get();
    } else {
      return NULL;
    }
  }

 private:
  bool has_selector_;
  WebKit::WebMediaStream stream_;
  WebKit::WebMediaStreamTrack component_;
  scoped_refptr<MockRTCStatsResponse> response_;
  bool request_succeeded_called_;
};

class MockPeerConnectionTracker : public PeerConnectionTracker {
 public:
  MOCK_METHOD1(UnregisterPeerConnection,
               void(RTCPeerConnectionHandler* pc_handler));
  // TODO(jiayl): add coverage for the following methods
  MOCK_METHOD2(TrackCreateOffer,
               void(RTCPeerConnectionHandler* pc_handler,
                    const RTCMediaConstraints& constraints));
  MOCK_METHOD2(TrackCreateAnswer,
               void(RTCPeerConnectionHandler* pc_handler,
                    const RTCMediaConstraints& constraints));
  MOCK_METHOD3(TrackSetSessionDescription,
               void(RTCPeerConnectionHandler* pc_handler,
                    const WebKit::WebRTCSessionDescription& desc,
                    Source source));
  MOCK_METHOD3(
      TrackUpdateIce,
      void(RTCPeerConnectionHandler* pc_handler,
           const std::vector<
               webrtc::PeerConnectionInterface::IceServer>& servers,
           const RTCMediaConstraints& options));
  MOCK_METHOD3(TrackAddIceCandidate,
               void(RTCPeerConnectionHandler* pc_handler,
                    const WebKit::WebRTCICECandidate& candidate,
                    Source source));
  MOCK_METHOD3(TrackAddStream,
               void(RTCPeerConnectionHandler* pc_handler,
                    const WebKit::WebMediaStream& stream,
                    Source source));
  MOCK_METHOD3(TrackRemoveStream,
               void(RTCPeerConnectionHandler* pc_handler,
                    const WebKit::WebMediaStream& stream,
                    Source source));
  MOCK_METHOD1(TrackOnIceComplete,
               void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD3(TrackCreateDataChannel,
               void(RTCPeerConnectionHandler* pc_handler,
                    const webrtc::DataChannelInterface* data_channel,
                    Source source));
  MOCK_METHOD1(TrackStop, void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD2(TrackSignalingStateChange,
               void(RTCPeerConnectionHandler* pc_handler,
                    WebRTCPeerConnectionHandlerClient::SignalingState state));
  MOCK_METHOD2(
      TrackIceConnectionStateChange,
      void(RTCPeerConnectionHandler* pc_handler,
           WebRTCPeerConnectionHandlerClient::ICEConnectionState state));
  MOCK_METHOD2(
      TrackIceGatheringStateChange,
      void(RTCPeerConnectionHandler* pc_handler,
           WebRTCPeerConnectionHandlerClient::ICEGatheringState state));
  MOCK_METHOD1(TrackOnRenegotiationNeeded,
               void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD2(TrackCreateDTMFSender,
               void(RTCPeerConnectionHandler* pc_handler,
                     const WebKit::WebMediaStreamTrack& track));
};

class RTCPeerConnectionHandlerUnderTest : public RTCPeerConnectionHandler {
 public:
  RTCPeerConnectionHandlerUnderTest(
      WebRTCPeerConnectionHandlerClient* client,
      MediaStreamDependencyFactory* dependency_factory)
      : RTCPeerConnectionHandler(client, dependency_factory) {
  }

  MockPeerConnectionImpl* native_peer_connection() {
    return static_cast<MockPeerConnectionImpl*>(native_peer_connection_.get());
  }
};

class RTCPeerConnectionHandlerTest : public ::testing::Test {
 public:
  RTCPeerConnectionHandlerTest() : mock_peer_connection_(NULL) {
  }

  virtual void SetUp() {
    mock_client_.reset(new NiceMock<MockWebRTCPeerConnectionHandlerClient>());
    mock_dependency_factory_.reset(new MockMediaStreamDependencyFactory());
    mock_dependency_factory_->EnsurePeerConnectionFactory();
    pc_handler_.reset(
        new RTCPeerConnectionHandlerUnderTest(mock_client_.get(),
                                              mock_dependency_factory_.get()));
    mock_tracker_.reset(new NiceMock<MockPeerConnectionTracker>());
    WebKit::WebRTCConfiguration config;
    WebKit::WebMediaConstraints constraints;
    EXPECT_TRUE(pc_handler_->InitializeForTest(config, constraints,
                                               mock_tracker_.get()));

    mock_peer_connection_ = pc_handler_->native_peer_connection();
    ASSERT_TRUE(mock_peer_connection_);
  }

  // Creates a WebKit local MediaStream.
  WebKit::WebMediaStream CreateLocalMediaStream(
      const std::string& stream_label) {
    std::string video_track_label("video-label");
    std::string audio_track_label("audio-label");

    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
        static_cast<size_t>(1));
    audio_sources[0].initialize(WebKit::WebString::fromUTF8(audio_track_label),
                                WebKit::WebMediaStreamSource::TypeAudio,
                                WebKit::WebString::fromUTF8("audio_track"));
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        static_cast<size_t>(1));
    video_sources[0].initialize(WebKit::WebString::fromUTF8(video_track_label),
                                WebKit::WebMediaStreamSource::TypeVideo,
                                WebKit::WebString::fromUTF8("video_track"));
    WebKit::WebMediaStream local_stream;
    local_stream.initialize(UTF8ToUTF16(stream_label), audio_sources,
                            video_sources);

    scoped_refptr<webrtc::MediaStreamInterface> native_stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label));
    WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
    local_stream.audioSources(audio_tracks);
    const std::string audio_track_id = UTF16ToUTF8(audio_tracks[0].id());
    scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        mock_dependency_factory_->CreateLocalAudioTrack(audio_track_id,
                                                        NULL));
    native_stream->AddTrack(audio_track);
    WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
    local_stream.audioSources(video_tracks);
    const std::string video_track_id = UTF16ToUTF8(video_tracks[0].id());
    webrtc::VideoSourceInterface* source = NULL;
    scoped_refptr<webrtc::VideoTrackInterface> video_track(
        mock_dependency_factory_->CreateLocalVideoTrack(
            video_track_id, source));
    native_stream->AddTrack(video_track);

    local_stream.setExtraData(new MediaStreamExtraData(native_stream, true));
    return local_stream;
  }

  // Creates a remote MediaStream and adds it to the mocked native
  // peer connection.
  scoped_refptr<webrtc::MediaStreamInterface>
  AddRemoteMockMediaStream(const std::string& stream_label,
                           const std::string& video_track_label,
                           const std::string& audio_track_label) {
    scoped_refptr<webrtc::MediaStreamInterface> stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label));
    if (!video_track_label.empty()) {
      webrtc::VideoSourceInterface* source = NULL;
      scoped_refptr<webrtc::VideoTrackInterface> video_track(
          mock_dependency_factory_->CreateLocalVideoTrack(
              video_track_label, source));
      stream->AddTrack(video_track);
    }
    if (!audio_track_label.empty()) {
      scoped_refptr<webrtc::AudioTrackInterface> audio_track(
          mock_dependency_factory_->CreateLocalAudioTrack(audio_track_label,
                                                          NULL));
      stream->AddTrack(audio_track);
    }
    mock_peer_connection_->AddRemoteStream(stream);
    return stream;
  }

  scoped_ptr<MockWebRTCPeerConnectionHandlerClient> mock_client_;
  scoped_ptr<MockMediaStreamDependencyFactory> mock_dependency_factory_;
  scoped_ptr<NiceMock<MockPeerConnectionTracker> > mock_tracker_;
  scoped_ptr<RTCPeerConnectionHandlerUnderTest> pc_handler_;

  // Weak reference to the mocked native peer connection implementation.
  MockPeerConnectionImpl* mock_peer_connection_;
};

TEST_F(RTCPeerConnectionHandlerTest, Destruct) {
  EXPECT_CALL(*mock_tracker_.get(), UnregisterPeerConnection(pc_handler_.get()))
      .Times(1);
  pc_handler_.reset(NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateOffer) {
  WebKit::WebRTCSessionDescriptionRequest request;
  WebKit::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateOffer(pc_handler_.get(), _));

  // TODO(perkj): Can WebKit::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != NULL);
  pc_handler_->createOffer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateAnswer) {
  WebKit::WebRTCSessionDescriptionRequest request;
  WebKit::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateAnswer(pc_handler_.get(), _));
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
  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), Ref(description),
                                         PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_peer_connection_, SetLocalDescription(_, _));

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

  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), Ref(description),
                                         PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_peer_connection_, SetRemoteDescription(_, _));

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

  EXPECT_CALL(*mock_tracker_.get(), TrackUpdateIce(pc_handler_.get(), _, _));
  // TODO(perkj): Test that the parameters in |config| can be translated when a
  // WebRTCConfiguration can be constructed. It's WebKit class and can't be
  // initialized from a test.
  EXPECT_TRUE(pc_handler_->updateICE(config, constraints));
}

TEST_F(RTCPeerConnectionHandlerTest, addICECandidate) {
  WebKit::WebRTCICECandidate candidate;
  candidate.initialize(kDummySdp, "mid", 1);

  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(),
                                   testing::Ref(candidate),
                                   PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_TRUE(pc_handler_->addICECandidate(candidate));
  EXPECT_EQ(kDummySdp, mock_peer_connection_->ice_sdp());
  EXPECT_EQ(1, mock_peer_connection_->sdp_mline_index());
  EXPECT_EQ("mid", mock_peer_connection_->sdp_mid());
}

TEST_F(RTCPeerConnectionHandlerTest, addAndRemoveStream) {
  std::string stream_label = "local_stream";
  WebKit::WebMediaStream local_stream(
      CreateLocalMediaStream(stream_label));
  WebKit::WebMediaConstraints constraints;

  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddStream(pc_handler_.get(),
                             testing::Ref(local_stream),
                             PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_tracker_.get(),
              TrackRemoveStream(pc_handler_.get(),
                                testing::Ref(local_stream),
                                PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_TRUE(pc_handler_->addStream(local_stream, constraints));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->GetAudioTracks().size());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->GetVideoTracks().size());

  pc_handler_->removeStream(local_stream);
  EXPECT_EQ(0u, mock_peer_connection_->local_streams()->count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsNoSelector) {
  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->getStats(request.get());
  // Note that callback gets executed synchronously by mock.
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsAfterClose) {
  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->stop();
  pc_handler_->getStats(request.get());
  // Note that callback gets executed synchronously by mock.
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithLocalSelector) {
  WebKit::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream"));
  WebKit::WebMediaConstraints constraints;
  pc_handler_->addStream(local_stream, constraints);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> tracks;
  local_stream.audioSources(tracks);
  ASSERT_LE(1ul, tracks.size());

  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(local_stream, tracks[0]);
  pc_handler_->getStats(request.get());
  EXPECT_EQ(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithRemoteSelector) {
  scoped_refptr<webrtc::MediaStreamInterface> stream(
      AddRemoteMockMediaStream("remote_stream", "video", "audio"));
  pc_handler_->OnAddStream(stream);
  const WebKit::WebMediaStream& remote_stream = mock_client_->remote_stream();

  WebKit::WebVector<WebKit::WebMediaStreamTrack> tracks;
  remote_stream.audioSources(tracks);
  ASSERT_LE(1ul, tracks.size());

  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(remote_stream, tracks[0]);
  pc_handler_->getStats(request.get());
  EXPECT_EQ(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithBadSelector) {
  // The setup is the same as GetStatsWithLocalSelector, but the stream is not
  // added to the PeerConnection.
  WebKit::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream_2"));
  WebKit::WebMediaConstraints constraints;
  WebKit::WebVector<WebKit::WebMediaStreamTrack> tracks;

  local_stream.audioSources(tracks);
  WebKit::WebMediaStreamTrack component = tracks[0];
  mock_peer_connection_->SetGetStatsResult(false);

  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(local_stream, component);
  pc_handler_->getStats(request.get());
  EXPECT_EQ(0, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, OnSignalingChange) {
  testing::InSequence sequence;

  webrtc::PeerConnectionInterface::SignalingState new_state =
      webrtc::PeerConnectionInterface::kHaveRemoteOffer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemoteOffer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemoteOffer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveLocalPrAnswer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalPrAnswer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalPrAnswer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveLocalOffer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalOffer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalOffer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveRemotePrAnswer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemotePrAnswer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemotePrAnswer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kClosed;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateClosed));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateClosed));
  pc_handler_->OnSignalingChange(new_state);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceConnectionChange) {
  testing::InSequence sequence;

  webrtc::PeerConnectionInterface::IceConnectionState new_state =
      webrtc::PeerConnectionInterface::kIceConnectionNew;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateStarting));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateStarting));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionChecking;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateChecking));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateChecking));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionConnected;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateConnected));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateConnected));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionCompleted;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionFailed;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateFailed));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateFailed));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionDisconnected;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateDisconnected));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateDisconnected));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionClosed;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateClosed));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateClosed));
  pc_handler_->OnIceConnectionChange(new_state);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceGatheringChange) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceGatheringStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateNew));
  EXPECT_CALL(*mock_client_.get(), didChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateNew));
  EXPECT_CALL(*mock_tracker_.get(), TrackIceGatheringStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateGathering));
  EXPECT_CALL(*mock_client_.get(), didChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateGathering));
  EXPECT_CALL(*mock_tracker_.get(), TrackIceGatheringStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete));
  EXPECT_CALL(*mock_client_.get(), didChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete));

  webrtc::PeerConnectionInterface::IceGatheringState new_state =
        webrtc::PeerConnectionInterface::kIceGatheringNew;
  pc_handler_->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringGathering;
  pc_handler_->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringComplete;
  pc_handler_->OnIceGatheringChange(new_state);

  // Check NULL candidate after ice gathering is completed.
  EXPECT_EQ("", mock_client_->candidate_mid());
  EXPECT_EQ(-1, mock_client_->candidate_mlineindex());
  EXPECT_EQ("", mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnAddAndOnRemoveStream) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(), TrackAddStream(
      pc_handler_.get(),
      testing::Property(&WebKit::WebMediaStream::label,
                        UTF8ToUTF16(remote_stream_label)),
      PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&WebKit::WebMediaStream::label,
                        UTF8ToUTF16(remote_stream_label))));

  EXPECT_CALL(*mock_tracker_.get(), TrackRemoveStream(
      pc_handler_.get(),
      testing::Property(&WebKit::WebMediaStream::label,
                        UTF8ToUTF16(remote_stream_label)),
      PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_client_.get(), didRemoveRemoteStream(
      testing::Property(&WebKit::WebMediaStream::label,
                        UTF8ToUTF16(remote_stream_label))));

  pc_handler_->OnAddStream(remote_stream);
  pc_handler_->OnRemoveStream(remote_stream);
}

// This test that WebKit is notified about remote track state changes.
TEST_F(RTCPeerConnectionHandlerTest, RemoteTrackState) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  testing::InSequence sequence;
  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&WebKit::WebMediaStream::label,
                        UTF8ToUTF16(remote_stream_label))));
  pc_handler_->OnAddStream(remote_stream);
  const WebKit::WebMediaStream& webkit_stream = mock_client_->remote_stream();

  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
  webkit_stream.audioTracks(audio_tracks);
  EXPECT_EQ(WebKit::WebMediaStreamSource::ReadyStateLive,
            audio_tracks[0].source().readyState());

  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
    webkit_stream.videoTracks(video_tracks);
  EXPECT_EQ(WebKit::WebMediaStreamSource::ReadyStateLive,
            video_tracks[0].source().readyState());

  remote_stream->GetAudioTracks()[0]->set_state(
      webrtc::MediaStreamTrackInterface::kEnded);
  EXPECT_EQ(WebKit::WebMediaStreamSource::ReadyStateEnded,
            audio_tracks[0].source().readyState());

  remote_stream->GetVideoTracks()[0]->set_state(
      webrtc::MediaStreamTrackInterface::kEnded);
  EXPECT_EQ(WebKit::WebMediaStreamSource::ReadyStateEnded,
            video_tracks[0].source().readyState());
}

TEST_F(RTCPeerConnectionHandlerTest, RemoveAndAddAudioTrackFromRemoteStream) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&WebKit::WebMediaStream::label,
                        UTF8ToUTF16(remote_stream_label))));
  pc_handler_->OnAddStream(remote_stream);
  const WebKit::WebMediaStream& webkit_stream = mock_client_->remote_stream();

  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
  webkit_stream.audioTracks(audio_tracks);
  EXPECT_EQ(1u, audio_tracks.size());

  // Remove the Webrtc audio track from the Webrtc MediaStream.
  scoped_refptr<webrtc::AudioTrackInterface> webrtc_track =
      remote_stream->GetAudioTracks()[0].get();
  remote_stream->RemoveTrack(webrtc_track);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> modified_audio_tracks1;
  webkit_stream.audioTracks(modified_audio_tracks1);
  EXPECT_EQ(0u, modified_audio_tracks1.size());

  // Add the WebRtc audio track again.
  remote_stream->AddTrack(webrtc_track);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> modified_audio_tracks2;
  webkit_stream.audioTracks(modified_audio_tracks2);
  EXPECT_EQ(1u, modified_audio_tracks2.size());
}

TEST_F(RTCPeerConnectionHandlerTest, RemoveAndAddVideoTrackFromRemoteStream) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "video"));

  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&WebKit::WebMediaStream::label,
                        UTF8ToUTF16(remote_stream_label))));
  pc_handler_->OnAddStream(remote_stream);
  const WebKit::WebMediaStream& webkit_stream = mock_client_->remote_stream();

  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
  webkit_stream.videoTracks(video_tracks);
  EXPECT_EQ(1u, video_tracks.size());

  // Remove the Webrtc video track from the Webrtc MediaStream.
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_track =
      remote_stream->GetVideoTracks()[0].get();
  remote_stream->RemoveTrack(webrtc_track);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> modified_video_tracks1;
  webkit_stream.videoTracks(modified_video_tracks1);
  EXPECT_EQ(0u, modified_video_tracks1.size());

  // Add the WebRtc video track again.
  remote_stream->AddTrack(webrtc_track);
  WebKit::WebVector<WebKit::WebMediaStreamTrack> modified_video_tracks2;
  webkit_stream.videoTracks(modified_video_tracks2);
  EXPECT_EQ(1u, modified_video_tracks2.size());
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceCandidate) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(), _,
                                   PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_client_.get(), didGenerateICECandidate(_));

  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory_->CreateIceCandidate("mid", 1, kDummySdp));
  pc_handler_->OnIceCandidate(native_candidate.get());
  EXPECT_EQ("mid", mock_client_->candidate_mid());
  EXPECT_EQ(1, mock_client_->candidate_mlineindex());
  EXPECT_EQ(kDummySdp, mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnRenegotiationNeeded) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackOnRenegotiationNeeded(pc_handler_.get()));
  EXPECT_CALL(*mock_client_.get(), negotiationNeeded());
  pc_handler_->OnRenegotiationNeeded();
}

TEST_F(RTCPeerConnectionHandlerTest, CreateDataChannel) {
  WebKit::WebString label = "d1";
  EXPECT_CALL(*mock_tracker_.get(),
              TrackCreateDataChannel(pc_handler_.get(),
                                     testing::NotNull(),
                                     PeerConnectionTracker::SOURCE_LOCAL));
  scoped_ptr<WebKit::WebRTCDataChannelHandler> channel(
      pc_handler_->createDataChannel("d1", true));
  EXPECT_TRUE(channel.get() != NULL);
  EXPECT_EQ(label, channel->label());
}

TEST_F(RTCPeerConnectionHandlerTest, CreateDtmfSender) {
  std::string stream_label = "local_stream";
  WebKit::WebMediaStream local_stream(CreateLocalMediaStream(stream_label));
  WebKit::WebMediaConstraints constraints;
  pc_handler_->addStream(local_stream, constraints);

  WebKit::WebVector<WebKit::WebMediaStreamTrack> tracks;
  local_stream.videoSources(tracks);

  ASSERT_LE(1ul, tracks.size());
  EXPECT_FALSE(pc_handler_->createDTMFSender(tracks[0]));

  local_stream.audioSources(tracks);
  ASSERT_LE(1ul, tracks.size());

  EXPECT_CALL(*mock_tracker_.get(),
              TrackCreateDTMFSender(pc_handler_.get(),
                                    testing::Ref(tracks[0])));

  scoped_ptr<WebKit::WebRTCDTMFSenderHandler> sender(
      pc_handler_->createDTMFSender(tracks[0]));
  EXPECT_TRUE(sender.get());
}

}  // namespace content
