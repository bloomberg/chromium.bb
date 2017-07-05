// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_peer_connection_handler.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_audio_device_factory.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/mock_data_channel_impl.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/processed_local_audio_source.h"
#include "content/renderer/media/webrtc/rtc_stats.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCConfiguration.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebRTCError.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/platform/WebRTCRtpReceiver.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"
#include "third_party/WebKit/public/platform/WebRTCVoidRequest.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebHeap.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/stats/test/rtcteststats.h"

static const char kDummySdp[] = "dummy sdp";
static const char kDummySdpType[] = "dummy type";

using blink::WebRTCPeerConnectionHandlerClient;
using testing::NiceMock;
using testing::_;
using testing::Ref;
using testing::SaveArg;

namespace content {

ACTION_P2(ExitMessageLoop, message_loop, quit_closure) {
  message_loop->task_runner()->PostTask(FROM_HERE, quit_closure);
}

class MockRTCStatsResponse : public LocalRTCStatsResponse {
 public:
  MockRTCStatsResponse()
      : report_count_(0),
        statistic_count_(0) {
  }

  void addStats(const blink::WebRTCLegacyStats& stats) override {
    ++report_count_;
    for (std::unique_ptr<blink::WebRTCLegacyStatsMemberIterator> member(
             stats.Iterator());
         !member->IsEnd(); member->Next()) {
      ++statistic_count_;
    }
  }

  int report_count() const { return report_count_; }

 private:
  int report_count_;
  int statistic_count_;
};

// Mocked wrapper for blink::WebRTCStatsRequest
class MockRTCStatsRequest : public LocalRTCStatsRequest {
 public:
  MockRTCStatsRequest()
      : has_selector_(false),
        request_succeeded_called_(false) {}

  bool hasSelector() const override { return has_selector_; }
  blink::WebMediaStreamTrack component() const override { return component_; }
  scoped_refptr<LocalRTCStatsResponse> createResponse() override {
    DCHECK(!response_.get());
    response_ = new rtc::RefCountedObject<MockRTCStatsResponse>();
    return response_;
  }

  void requestSucceeded(const LocalRTCStatsResponse* response) override {
    EXPECT_EQ(response, response_.get());
    request_succeeded_called_ = true;
  }

  // Function for setting whether or not a selector is available.
  void setSelector(const blink::WebMediaStreamTrack& component) {
    has_selector_ = true;
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
  blink::WebMediaStreamTrack component_;
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
                    const blink::WebMediaConstraints& constraints));
  MOCK_METHOD2(TrackCreateAnswer,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebMediaConstraints& constraints));
  MOCK_METHOD4(TrackSetSessionDescription,
               void(RTCPeerConnectionHandler* pc_handler,
                    const std::string& sdp, const std::string& type,
                    Source source));
  MOCK_METHOD2(
      TrackSetConfiguration,
      void(RTCPeerConnectionHandler* pc_handler,
           const webrtc::PeerConnectionInterface::RTCConfiguration& config));
  MOCK_METHOD4(TrackAddIceCandidate,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebRTCICECandidate& candidate,
                    Source source,
                    bool succeeded));
  MOCK_METHOD3(TrackAddStream,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebMediaStream& stream,
                    Source source));
  MOCK_METHOD3(TrackRemoveStream,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebMediaStream& stream,
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
  MOCK_METHOD4(TrackSessionDescriptionCallback,
               void(RTCPeerConnectionHandler* pc_handler,
                    Action action,
                    const std::string& type,
                    const std::string& value));
  MOCK_METHOD1(TrackOnRenegotiationNeeded,
               void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD2(TrackCreateDTMFSender,
               void(RTCPeerConnectionHandler* pc_handler,
                     const blink::WebMediaStreamTrack& track));
};

class MockRTCStatsReportCallback : public blink::WebRTCStatsReportCallback {
 public:
  explicit MockRTCStatsReportCallback(
      std::unique_ptr<blink::WebRTCStatsReport>* result)
      : main_thread_(base::ThreadTaskRunnerHandle::Get()), result_(result) {
    DCHECK(result_);
  }

  void OnStatsDelivered(
      std::unique_ptr<blink::WebRTCStatsReport> report) override {
    EXPECT_TRUE(main_thread_->BelongsToCurrentThread());
    EXPECT_TRUE(report);
    result_->reset(report.release());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  std::unique_ptr<blink::WebRTCStatsReport>* result_;
};

template<typename T>
std::vector<T> ToSequence(T value) {
  std::vector<T> vec;
  vec.push_back(value);
  return vec;
}

template<typename T>
void ExpectSequenceEquals(const blink::WebVector<T>& sequence, T value) {
  EXPECT_EQ(sequence.size(), static_cast<size_t>(1));
  EXPECT_EQ(sequence[0], value);
}

class RTCPeerConnectionHandlerUnderTest : public RTCPeerConnectionHandler {
 public:
  RTCPeerConnectionHandlerUnderTest(
      WebRTCPeerConnectionHandlerClient* client,
      PeerConnectionDependencyFactory* dependency_factory)
      : RTCPeerConnectionHandler(client, dependency_factory) {
  }

  MockPeerConnectionImpl* native_peer_connection() {
    return static_cast<MockPeerConnectionImpl*>(
        RTCPeerConnectionHandler::native_peer_connection());
  }

  webrtc::PeerConnectionObserver* observer() {
    return native_peer_connection()->observer();
  }
};

class RTCPeerConnectionHandlerTest : public ::testing::Test {
 public:
  RTCPeerConnectionHandlerTest() : mock_peer_connection_(NULL) {
    child_process_.reset(new ChildProcess());
  }

  void SetUp() override {
    mock_client_.reset(new NiceMock<MockWebRTCPeerConnectionHandlerClient>());
    mock_dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    pc_handler_.reset(
        new RTCPeerConnectionHandlerUnderTest(
            mock_client_.get(), mock_dependency_factory_.get()));
    mock_tracker_.reset(new NiceMock<MockPeerConnectionTracker>());
    blink::WebRTCConfiguration config;
    blink::WebMediaConstraints constraints;
    EXPECT_TRUE(pc_handler_->InitializeForTest(
        config, constraints, mock_tracker_.get()->AsWeakPtr()));

    mock_peer_connection_ = pc_handler_->native_peer_connection();
    ASSERT_TRUE(mock_peer_connection_);
    EXPECT_CALL(*mock_peer_connection_, Close());
  }

  void TearDown() override {
    pc_handler_.reset();
    mock_tracker_.reset();
    mock_dependency_factory_.reset();
    mock_client_.reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  // Creates a WebKit local MediaStream.
  blink::WebMediaStream CreateLocalMediaStream(
      const std::string& stream_label) {
    std::string video_track_label("video-label");
    std::string audio_track_label("audio-label");
    blink::WebMediaStreamSource blink_audio_source;
    blink_audio_source.Initialize(blink::WebString::FromUTF8(audio_track_label),
                                  blink::WebMediaStreamSource::kTypeAudio,
                                  blink::WebString::FromUTF8("audio_track"),
                                  false /* remote */);
    ProcessedLocalAudioSource* const audio_source =
        new ProcessedLocalAudioSource(
            -1 /* consumer_render_frame_id is N/A for non-browser tests */,
            StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE, "Mock device",
                             "mock_device_id",
                             media::AudioParameters::kAudioCDSampleRate,
                             media::CHANNEL_LAYOUT_STEREO,
                             media::AudioParameters::kAudioCDSampleRate / 100),
            AudioProcessingProperties(),
            base::Bind(&RTCPeerConnectionHandlerTest::OnAudioSourceStarted),
            mock_dependency_factory_.get());
    audio_source->SetAllowInvalidRenderFrameIdForTesting(true);
    blink_audio_source.SetExtraData(audio_source);  // Takes ownership.

    blink::WebMediaStreamSource video_source;
    video_source.Initialize(blink::WebString::FromUTF8(video_track_label),
                            blink::WebMediaStreamSource::kTypeVideo,
                            blink::WebString::FromUTF8("video_track"),
                            false /* remote */);
    MockMediaStreamVideoSource* native_video_source =
        new MockMediaStreamVideoSource();
    video_source.SetExtraData(native_video_source);

    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks(
        static_cast<size_t>(1));
    audio_tracks[0].Initialize(blink_audio_source.Id(), blink_audio_source);
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(),
                Initialize(_, _, -1));
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(),
                SetAutomaticGainControl(true));
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(), Start());
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(), Stop());
    CHECK(audio_source->ConnectToTrack(audio_tracks[0]));
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks(
        static_cast<size_t>(1));
    video_tracks[0] = MediaStreamVideoTrack::CreateVideoTrack(
        native_video_source, MediaStreamVideoSource::ConstraintsCallback(),
        true);

    blink::WebMediaStream local_stream;
    local_stream.Initialize(blink::WebString::FromUTF8(stream_label),
                            audio_tracks, video_tracks);
    local_stream.SetExtraData(new MediaStream());
    return local_stream;
  }

  // Creates a remote MediaStream and adds it to the mocked native
  // peer connection.
  rtc::scoped_refptr<webrtc::MediaStreamInterface> AddRemoteMockMediaStream(
      const std::string& stream_label,
      const std::string& video_track_label,
      const std::string& audio_track_label) {
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label).get());
    if (!video_track_label.empty()) {
      InvokeAddTrack(stream,
                     MockWebRtcVideoTrack::Create(video_track_label).get());
    }
    if (!audio_track_label.empty()) {
      InvokeAddTrack(stream,
                     MockWebRtcAudioTrack::Create(audio_track_label).get());
    }
    mock_peer_connection_->AddRemoteStream(stream);
    return stream;
  }

  void StopAllTracks(const blink::WebMediaStream& stream) {
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    stream.AudioTracks(audio_tracks);
    for (const auto& track : audio_tracks)
      MediaStreamAudioTrack::From(track)->Stop();

    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    stream.VideoTracks(video_tracks);
    for (const auto& track : video_tracks)
      MediaStreamVideoTrack::GetVideoTrack(track)->Stop();
  }

  static void OnAudioSourceStarted(MediaStreamSource* source,
                                   MediaStreamRequestResult result,
                                   const blink::WebString& result_name) {}

  void InvokeOnAddStream(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream) {
    InvokeOnSignalingThread(
        base::Bind(&webrtc::PeerConnectionObserver::OnAddStream,
                   base::Unretained(pc_handler_->observer()), remote_stream));
  }

  void InvokeOnRemoveStream(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream) {
    InvokeOnSignalingThread(
        base::Bind(&webrtc::PeerConnectionObserver::OnRemoveStream,
                   base::Unretained(pc_handler_->observer()), remote_stream));
  }

  template <typename T>
  void InvokeAddTrack(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream,
      T* webrtc_track) {
    InvokeOnSignalingThread(base::Bind(
        [](webrtc::MediaStreamInterface* remote_stream, T* webrtc_track) {
          EXPECT_TRUE(remote_stream->AddTrack(webrtc_track));
        },
        base::Unretained(remote_stream.get()), base::Unretained(webrtc_track)));
  }

  template <typename T>
  void InvokeRemoveTrack(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream,
      T* webrtc_track) {
    InvokeOnSignalingThread(base::Bind(
        [](webrtc::MediaStreamInterface* remote_stream, T* webrtc_track) {
          EXPECT_TRUE(remote_stream->RemoveTrack(webrtc_track));
        },
        base::Unretained(remote_stream.get()), base::Unretained(webrtc_track)));
  }

  template <typename T>
  void InvokeOnSignalingThread(T callback) {
    mock_dependency_factory_->GetWebRtcSignalingThread()->PostTask(FROM_HERE,
                                                                   callback);
    RunMessageLoopsUntilIdle();
  }

  // Wait for all current posts to the webrtc signaling thread to run and then
  // run the message loop until idle on the main thread.
  void RunMessageLoopsUntilIdle() {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    mock_dependency_factory_->GetWebRtcSignalingThread()->PostTask(
        FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                              base::Unretained(&waitable_event)));
    waitable_event.Wait();
    base::RunLoop().RunUntilIdle();
  }

 private:
  void SignalWaitableEvent(base::WaitableEvent* waitable_event) {
    waitable_event->Signal();
  }

 public:
  base::MessageLoop message_loop_;
  std::unique_ptr<ChildProcess> child_process_;
  std::unique_ptr<MockWebRTCPeerConnectionHandlerClient> mock_client_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> mock_dependency_factory_;
  std::unique_ptr<NiceMock<MockPeerConnectionTracker>> mock_tracker_;
  std::unique_ptr<RTCPeerConnectionHandlerUnderTest> pc_handler_;
  MockAudioDeviceFactory mock_audio_device_factory_;

  // Weak reference to the mocked native peer connection implementation.
  MockPeerConnectionImpl* mock_peer_connection_;
};

TEST_F(RTCPeerConnectionHandlerTest, Destruct) {
  EXPECT_CALL(*mock_tracker_.get(), UnregisterPeerConnection(pc_handler_.get()))
      .Times(1);
  pc_handler_.reset(NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, NoCallbacksToClientAfterStop) {
  pc_handler_->Stop();

  EXPECT_CALL(*mock_client_.get(), NegotiationNeeded()).Times(0);
  pc_handler_->observer()->OnRenegotiationNeeded();

  EXPECT_CALL(*mock_client_.get(), DidGenerateICECandidate(_)).Times(0);
  std::unique_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory_->CreateIceCandidate("sdpMid", 1, kDummySdp));
  pc_handler_->observer()->OnIceCandidate(native_candidate.get());

  EXPECT_CALL(*mock_client_.get(), DidChangeSignalingState(_)).Times(0);
  pc_handler_->observer()->OnSignalingChange(
      webrtc::PeerConnectionInterface::kHaveRemoteOffer);

  EXPECT_CALL(*mock_client_.get(), DidChangeICEGatheringState(_)).Times(0);
  pc_handler_->observer()->OnIceGatheringChange(
      webrtc::PeerConnectionInterface::kIceGatheringNew);

  EXPECT_CALL(*mock_client_.get(), DidChangeICEConnectionState(_)).Times(0);
  pc_handler_->observer()->OnIceConnectionChange(
      webrtc::PeerConnectionInterface::kIceConnectionDisconnected);

  EXPECT_CALL(*mock_client_.get(), DidAddRemoteStream(_)).Times(0);
  std::string remote_stream_label("remote_stream");
  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));
  InvokeOnAddStream(remote_stream);

  EXPECT_CALL(*mock_client_.get(), DidRemoveRemoteStream(_)).Times(0);
  InvokeOnRemoveStream(remote_stream);

  EXPECT_CALL(*mock_client_.get(), DidAddRemoteDataChannel(_)).Times(0);
  webrtc::DataChannelInit config;
  rtc::scoped_refptr<webrtc::DataChannelInterface> remote_data_channel(
      new rtc::RefCountedObject<MockDataChannel>("dummy", &config));
  pc_handler_->observer()->OnDataChannel(remote_data_channel);

  RunMessageLoopsUntilIdle();
}

TEST_F(RTCPeerConnectionHandlerTest, DestructAllHandlers) {
  EXPECT_CALL(*mock_client_.get(), ReleasePeerConnectionHandler()).Times(1);
  RTCPeerConnectionHandler::DestructAllHandlers();
}

TEST_F(RTCPeerConnectionHandlerTest, CreateOffer) {
  blink::WebRTCSessionDescriptionRequest request;
  blink::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateOffer(pc_handler_.get(), _));

  // TODO(perkj): Can blink::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != NULL);
  pc_handler_->CreateOffer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateAnswer) {
  blink::WebRTCSessionDescriptionRequest request;
  blink::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateAnswer(pc_handler_.get(), _));
  // TODO(perkj): Can blink::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != NULL);
  pc_handler_->CreateAnswer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, setLocalDescription) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);
  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), kDummySdp,
                                         kDummySdpType,
                                         PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_peer_connection_, SetLocalDescription(_, _));

  pc_handler_->SetLocalDescription(request, description);
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(description.GetType(), pc_handler_->LocalDescription().GetType());
  EXPECT_EQ(description.Sdp(), pc_handler_->LocalDescription().Sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->local_description() != NULL);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->local_description()->type());
  mock_peer_connection_->local_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);

  // TODO(deadbeef): Also mock the "success" callback from the PeerConnection
  // and ensure that the sucessful result is tracked by PeerConnectionTracker.
}

// Test that setLocalDescription with invalid SDP will result in a failure, and
// is tracked as a failure with PeerConnectionTracker.
TEST_F(RTCPeerConnectionHandlerTest, setLocalDescriptionParseError) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);
  testing::InSequence sequence;
  // Expect two "Track" calls, one for the start of the attempt and one for the
  // failure.
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSetSessionDescription(pc_handler_.get(), kDummySdp, kDummySdpType,
                                 PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSessionDescriptionCallback(
          pc_handler_.get(),
          PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION, "OnFailure", _));

  // Used to simulate a parse failure.
  mock_dependency_factory_->SetFailToCreateSessionDescription(true);
  pc_handler_->SetLocalDescription(request, description);
  RunMessageLoopsUntilIdle();
  // A description that failed to be applied shouldn't be stored.
  EXPECT_TRUE(pc_handler_->LocalDescription().Sdp().IsEmpty());
}

TEST_F(RTCPeerConnectionHandlerTest, setRemoteDescription) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);

  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), kDummySdp,
                                         kDummySdpType,
                                         PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_peer_connection_, SetRemoteDescription(_, _));

  pc_handler_->SetRemoteDescription(request, description);
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(description.GetType(), pc_handler_->RemoteDescription().GetType());
  EXPECT_EQ(description.Sdp(), pc_handler_->RemoteDescription().Sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->remote_description() != NULL);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->remote_description()->type());
  mock_peer_connection_->remote_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);

  // TODO(deadbeef): Also mock the "success" callback from the PeerConnection
  // and ensure that the sucessful result is tracked by PeerConnectionTracker.
}

// Test that setRemoteDescription with invalid SDP will result in a failure, and
// is tracked as a failure with PeerConnectionTracker.
TEST_F(RTCPeerConnectionHandlerTest, setRemoteDescriptionParseError) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);
  testing::InSequence sequence;
  // Expect two "Track" calls, one for the start of the attempt and one for the
  // failure.
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSetSessionDescription(pc_handler_.get(), kDummySdp, kDummySdpType,
                                 PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSessionDescriptionCallback(
                  pc_handler_.get(),
                  PeerConnectionTracker::ACTION_SET_REMOTE_DESCRIPTION,
                  "OnFailure", _));

  // Used to simulate a parse failure.
  mock_dependency_factory_->SetFailToCreateSessionDescription(true);
  pc_handler_->SetRemoteDescription(request, description);
  RunMessageLoopsUntilIdle();
  // A description that failed to be applied shouldn't be stored.
  EXPECT_TRUE(pc_handler_->RemoteDescription().Sdp().IsEmpty());
}

TEST_F(RTCPeerConnectionHandlerTest, setConfiguration) {
  blink::WebRTCConfiguration config;

  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetConfiguration(pc_handler_.get(), _));
  // TODO(perkj): Test that the parameters in |config| can be translated when a
  // WebRTCConfiguration can be constructed. It's WebKit class and can't be
  // initialized from a test.
  EXPECT_EQ(blink::WebRTCErrorType::kNone,
            pc_handler_->SetConfiguration(config));
}

// Test that when an error occurs in SetConfiguration, it's converted to a
// blink error and false is returned.
TEST_F(RTCPeerConnectionHandlerTest, setConfigurationError) {
  blink::WebRTCConfiguration config;

  mock_peer_connection_->set_setconfiguration_error_type(
      webrtc::RTCErrorType::INVALID_MODIFICATION);
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetConfiguration(pc_handler_.get(), _));
  EXPECT_EQ(blink::WebRTCErrorType::kInvalidModification,
            pc_handler_->SetConfiguration(config));
}

TEST_F(RTCPeerConnectionHandlerTest, addICECandidate) {
  blink::WebRTCICECandidate candidate;
  candidate.Initialize(kDummySdp, "sdpMid", 1);

  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(),
                                   testing::Ref(candidate),
                                   PeerConnectionTracker::SOURCE_REMOTE,
                                   true));
  EXPECT_TRUE(pc_handler_->AddICECandidate(candidate));
  EXPECT_EQ(kDummySdp, mock_peer_connection_->ice_sdp());
  EXPECT_EQ(1, mock_peer_connection_->sdp_mline_index());
  EXPECT_EQ("sdpMid", mock_peer_connection_->sdp_mid());
}

TEST_F(RTCPeerConnectionHandlerTest, addAndRemoveStream) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream(stream_label));
  blink::WebMediaConstraints constraints;

  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddStream(pc_handler_.get(),
                             testing::Ref(local_stream),
                             PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_tracker_.get(),
              TrackRemoveStream(pc_handler_.get(),
                                testing::Ref(local_stream),
                                PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_TRUE(pc_handler_->AddStream(local_stream, constraints));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->GetAudioTracks().size());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->GetVideoTracks().size());

  EXPECT_FALSE(pc_handler_->AddStream(local_stream, constraints));

  pc_handler_->RemoveStream(local_stream);
  EXPECT_EQ(0u, mock_peer_connection_->local_streams()->count());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, addStreamWithStoppedAudioAndVideoTrack) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream(stream_label));
  blink::WebMediaConstraints constraints;

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  local_stream.AudioTracks(audio_tracks);
  MediaStreamAudioSource* native_audio_source =
      MediaStreamAudioSource::From(audio_tracks[0].Source());
  native_audio_source->StopSource();

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  local_stream.VideoTracks(video_tracks);
  MediaStreamVideoSource* native_video_source =
      static_cast<MediaStreamVideoSource*>(
          video_tracks[0].Source().GetExtraData());
  native_video_source->StopSource();

  EXPECT_TRUE(pc_handler_->AddStream(local_stream, constraints));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(
      1u,
      mock_peer_connection_->local_streams()->at(0)->GetAudioTracks().size());
  EXPECT_EQ(
      1u,
      mock_peer_connection_->local_streams()->at(0)->GetVideoTracks().size());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsNoSelector) {
  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsAfterClose) {
  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->Stop();
  RunMessageLoopsUntilIdle();
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithLocalSelector) {
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream"));
  blink::WebMediaConstraints constraints;
  pc_handler_->AddStream(local_stream, constraints);
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  local_stream.AudioTracks(tracks);
  ASSERT_LE(1ul, tracks.size());

  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(tracks[0]);
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(1, request->result()->report_count());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithRemoteSelector) {
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream(
      AddRemoteMockMediaStream("remote_stream", "video", "audio"));
  InvokeOnAddStream(stream);
  const blink::WebMediaStream& remote_stream = mock_client_->remote_stream();

  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  remote_stream.AudioTracks(tracks);
  ASSERT_LE(1ul, tracks.size());

  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(tracks[0]);
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithBadSelector) {
  // The setup is the same as GetStatsWithLocalSelector, but the stream is not
  // added to the PeerConnection.
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream_2"));
  blink::WebMediaConstraints constraints;
  blink::WebVector<blink::WebMediaStreamTrack> tracks;

  local_stream.AudioTracks(tracks);
  blink::WebMediaStreamTrack component = tracks[0];
  mock_peer_connection_->SetGetStatsResult(false);

  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(component);
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(0, request->result()->report_count());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, GetRTCStats) {
  WhitelistStatsForTesting(webrtc::RTCTestStats::kType);

  rtc::scoped_refptr<webrtc::RTCStatsReport> report =
      webrtc::RTCStatsReport::Create();

  report->AddStats(std::unique_ptr<const webrtc::RTCStats>(
      new webrtc::RTCTestStats("RTCUndefinedStats", 1000)));

  std::unique_ptr<webrtc::RTCTestStats> stats_defined_members(
      new webrtc::RTCTestStats("RTCDefinedStats", 2000));
  stats_defined_members->m_bool = true;
  stats_defined_members->m_int32 = 42;
  stats_defined_members->m_uint32 = 42;
  stats_defined_members->m_int64 = 42;
  stats_defined_members->m_uint64 = 42;
  stats_defined_members->m_double = 42.0;
  stats_defined_members->m_string = "42";
  stats_defined_members->m_sequence_bool = ToSequence<bool>(true);
  stats_defined_members->m_sequence_int32 = ToSequence<int32_t>(42);
  stats_defined_members->m_sequence_uint32 = ToSequence<uint32_t>(42);
  stats_defined_members->m_sequence_int64 = ToSequence<int64_t>(42);
  stats_defined_members->m_sequence_uint64 = ToSequence<uint64_t>(42);
  stats_defined_members->m_sequence_double = ToSequence<double>(42);
  stats_defined_members->m_sequence_string = ToSequence<std::string>("42");
  report->AddStats(std::unique_ptr<const webrtc::RTCStats>(
      stats_defined_members.release()));

  pc_handler_->native_peer_connection()->SetGetStatsReport(report);
  std::unique_ptr<blink::WebRTCStatsReport> result;
  pc_handler_->GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback>(
      new MockRTCStatsReportCallback(&result)));
  RunMessageLoopsUntilIdle();
  EXPECT_TRUE(result);

  int undefined_stats_count = 0;
  int defined_stats_count = 0;
  for (std::unique_ptr<blink::WebRTCStats> stats = result->Next(); stats;
       stats.reset(result->Next().release())) {
    EXPECT_EQ(stats->GetType().Utf8(), webrtc::RTCTestStats::kType);
    if (stats->Id().Utf8() == "RTCUndefinedStats") {
      ++undefined_stats_count;
      EXPECT_EQ(stats->Timestamp(), 1.0);
      for (size_t i = 0; i < stats->MembersCount(); ++i) {
        EXPECT_FALSE(stats->GetMember(i)->IsDefined());
      }
    } else if (stats->Id().Utf8() == "RTCDefinedStats") {
      ++defined_stats_count;
      EXPECT_EQ(stats->Timestamp(), 2.0);
      std::set<blink::WebRTCStatsMemberType> members;
      for (size_t i = 0; i < stats->MembersCount(); ++i) {
        std::unique_ptr<blink::WebRTCStatsMember> member = stats->GetMember(i);
        // TODO(hbos): A WebRTC-change is adding new members, this would cause
        // not all members to be defined. This if-statement saves Chromium from
        // crashing. As soon as the change has been rolled in, I will update
        // this test. crbug.com/627816
        if (!member->IsDefined())
          continue;
        EXPECT_TRUE(member->IsDefined());
        members.insert(member->GetType());
        switch (member->GetType()) {
          case blink::kWebRTCStatsMemberTypeBool:
            EXPECT_EQ(member->ValueBool(), true);
            break;
          case blink::kWebRTCStatsMemberTypeInt32:
            EXPECT_EQ(member->ValueInt32(), static_cast<int32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeUint32:
            EXPECT_EQ(member->ValueUint32(), static_cast<uint32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeInt64:
            EXPECT_EQ(member->ValueInt64(), static_cast<int64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeUint64:
            EXPECT_EQ(member->ValueUint64(), static_cast<uint64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeDouble:
            EXPECT_EQ(member->ValueDouble(), 42.0);
            break;
          case blink::kWebRTCStatsMemberTypeString:
            EXPECT_EQ(member->ValueString(), blink::WebString::FromUTF8("42"));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceBool:
            ExpectSequenceEquals(member->ValueSequenceBool(), 1);
            break;
          case blink::kWebRTCStatsMemberTypeSequenceInt32:
            ExpectSequenceEquals(member->ValueSequenceInt32(),
                                 static_cast<int32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceUint32:
            ExpectSequenceEquals(member->ValueSequenceUint32(),
                                 static_cast<uint32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceInt64:
            ExpectSequenceEquals(member->ValueSequenceInt64(),
                                 static_cast<int64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceUint64:
            ExpectSequenceEquals(member->ValueSequenceUint64(),
                                 static_cast<uint64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceDouble:
            ExpectSequenceEquals(member->ValueSequenceDouble(), 42.0);
            break;
          case blink::kWebRTCStatsMemberTypeSequenceString:
            ExpectSequenceEquals(member->ValueSequenceString(),
                                 blink::WebString::FromUTF8("42"));
            break;
          default:
            NOTREACHED();
        }
      }
      EXPECT_EQ(members.size(), static_cast<size_t>(14));
    } else {
      NOTREACHED();
    }
  }
  EXPECT_EQ(undefined_stats_count, 1);
  EXPECT_EQ(defined_stats_count, 1);
}

TEST_F(RTCPeerConnectionHandlerTest, GetReceivers) {
  std::vector<blink::WebMediaStream> remote_streams;

  InvokeOnAddStream(AddRemoteMockMediaStream("stream0", "video0", "audio0"));
  remote_streams.push_back(mock_client_->remote_stream());
  InvokeOnAddStream(AddRemoteMockMediaStream("stream1", "video1", "audio1"));
  remote_streams.push_back(mock_client_->remote_stream());
  InvokeOnAddStream(AddRemoteMockMediaStream("stream2", "video2", "audio2"));
  remote_streams.push_back(mock_client_->remote_stream());

  std::set<std::string> expected_remote_track_ids;
  expected_remote_track_ids.insert("video0");
  expected_remote_track_ids.insert("audio0");
  expected_remote_track_ids.insert("video1");
  expected_remote_track_ids.insert("audio1");
  expected_remote_track_ids.insert("video2");
  expected_remote_track_ids.insert("audio2");

  std::set<std::string> remote_track_ids;
  for (const auto& remote_stream : remote_streams) {
    blink::WebVector<blink::WebMediaStreamTrack> tracks;
    remote_stream.AudioTracks(tracks);
    for (const auto& audio_track : tracks) {
      remote_track_ids.insert(audio_track.Id().Utf8());
    }
    remote_stream.VideoTracks(tracks);
    for (const auto& video_track : tracks) {
      remote_track_ids.insert(video_track.Id().Utf8());
    }
  }
  EXPECT_EQ(expected_remote_track_ids, remote_track_ids);

  blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>> receivers =
      pc_handler_->GetReceivers();
  EXPECT_EQ(remote_track_ids.size(), receivers.size());
  std::set<uintptr_t> receiver_ids;
  std::set<std::string> receiver_track_ids;
  for (const auto& receiver : receivers) {
    receiver_ids.insert(receiver->Id());
    receiver_track_ids.insert(receiver->Track().Id().Utf8());
  }
  EXPECT_EQ(expected_remote_track_ids.size(), receiver_ids.size());
  EXPECT_EQ(expected_remote_track_ids.size(), receiver_track_ids.size());
}

TEST_F(RTCPeerConnectionHandlerTest, OnSignalingChange) {
  testing::InSequence sequence;

  webrtc::PeerConnectionInterface::SignalingState new_state =
      webrtc::PeerConnectionInterface::kHaveRemoteOffer;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSignalingStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kSignalingStateHaveRemoteOffer));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeSignalingState(
          WebRTCPeerConnectionHandlerClient::kSignalingStateHaveRemoteOffer));
  pc_handler_->observer()->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveLocalPrAnswer;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSignalingStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kSignalingStateHaveLocalPrAnswer));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeSignalingState(
          WebRTCPeerConnectionHandlerClient::kSignalingStateHaveLocalPrAnswer));
  pc_handler_->observer()->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveLocalOffer;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSignalingStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kSignalingStateHaveLocalOffer));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeSignalingState(
          WebRTCPeerConnectionHandlerClient::kSignalingStateHaveLocalOffer));
  pc_handler_->observer()->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveRemotePrAnswer;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSignalingStateChange(pc_handler_.get(),
                                        WebRTCPeerConnectionHandlerClient::
                                            kSignalingStateHaveRemotePrAnswer));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeSignalingState(WebRTCPeerConnectionHandlerClient::
                                          kSignalingStateHaveRemotePrAnswer));
  pc_handler_->observer()->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kClosed;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSignalingStateChange(
                  pc_handler_.get(),
                  WebRTCPeerConnectionHandlerClient::kSignalingStateClosed));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeSignalingState(
                  WebRTCPeerConnectionHandlerClient::kSignalingStateClosed));
  pc_handler_->observer()->OnSignalingChange(new_state);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceConnectionChange) {
  testing::InSequence sequence;

  webrtc::PeerConnectionInterface::IceConnectionState new_state =
      webrtc::PeerConnectionInterface::kIceConnectionNew;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceConnectionStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateStarting));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEConnectionState(
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateStarting));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionChecking;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceConnectionStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateChecking));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEConnectionState(
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateChecking));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionConnected;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceConnectionStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateConnected));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEConnectionState(
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateConnected));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionCompleted;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceConnectionStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateCompleted));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEConnectionState(
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateCompleted));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionFailed;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceConnectionStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateFailed));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEConnectionState(
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateFailed));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionDisconnected;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceConnectionStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateDisconnected));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEConnectionState(
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateDisconnected));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionClosed;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceConnectionStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateClosed));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEConnectionState(
          WebRTCPeerConnectionHandlerClient::kICEConnectionStateClosed));
  pc_handler_->observer()->OnIceConnectionChange(new_state);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceGatheringChange) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceGatheringStateChange(
                  pc_handler_.get(),
                  WebRTCPeerConnectionHandlerClient::kICEGatheringStateNew));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeICEGatheringState(
                  WebRTCPeerConnectionHandlerClient::kICEGatheringStateNew));
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceGatheringStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEGatheringStateGathering));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEGatheringState(
          WebRTCPeerConnectionHandlerClient::kICEGatheringStateGathering));
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackIceGatheringStateChange(
          pc_handler_.get(),
          WebRTCPeerConnectionHandlerClient::kICEGatheringStateComplete));
  EXPECT_CALL(
      *mock_client_.get(),
      DidChangeICEGatheringState(
          WebRTCPeerConnectionHandlerClient::kICEGatheringStateComplete));

  webrtc::PeerConnectionInterface::IceGatheringState new_state =
        webrtc::PeerConnectionInterface::kIceGatheringNew;
  pc_handler_->observer()->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringGathering;
  pc_handler_->observer()->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringComplete;
  pc_handler_->observer()->OnIceGatheringChange(new_state);

  // Check NULL candidate after ice gathering is completed.
  EXPECT_EQ("", mock_client_->candidate_mid());
  EXPECT_EQ(-1, mock_client_->candidate_mlineindex());
  EXPECT_EQ("", mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnAddAndOnRemoveStream) {
  std::string remote_stream_label("remote_stream");
  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  testing::InSequence sequence;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackAddStream(
          pc_handler_.get(),
          testing::Property(&blink::WebMediaStream::Id,
                            blink::WebString::FromASCII(remote_stream_label)),
          PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_client_.get(),
              DidAddRemoteStream(testing::Property(
                  &blink::WebMediaStream::Id,
                  blink::WebString::FromASCII(remote_stream_label))));

  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackRemoveStream(
          pc_handler_.get(),
          testing::Property(&blink::WebMediaStream::Id,
                            blink::WebString::FromASCII(remote_stream_label)),
          PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_client_.get(),
              DidRemoveRemoteStream(testing::Property(
                  &blink::WebMediaStream::Id,
                  blink::WebString::FromASCII(remote_stream_label))));

  InvokeOnAddStream(remote_stream);
  InvokeOnRemoveStream(remote_stream);
}

// This test that WebKit is notified about remote track state changes.
TEST_F(RTCPeerConnectionHandlerTest, RemoteTrackState) {
  std::string remote_stream_label("remote_stream");
  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  testing::InSequence sequence;
  EXPECT_CALL(*mock_client_.get(),
              DidAddRemoteStream(testing::Property(
                  &blink::WebMediaStream::Id,
                  blink::WebString::FromASCII(remote_stream_label))));
  InvokeOnAddStream(remote_stream);
  const blink::WebMediaStream& webkit_stream = mock_client_->remote_stream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  webkit_stream.AudioTracks(audio_tracks);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            audio_tracks[0].Source().GetReadyState());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  webkit_stream.VideoTracks(video_tracks);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            video_tracks[0].Source().GetReadyState());

  InvokeOnSignalingThread(
      base::Bind(&MockWebRtcAudioTrack::SetEnded,
                 base::Unretained(static_cast<MockWebRtcAudioTrack*>(
                     remote_stream->GetAudioTracks()[0].get()))));
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded,
            audio_tracks[0].Source().GetReadyState());

  InvokeOnSignalingThread(
      base::Bind(&MockWebRtcVideoTrack::SetEnded,
                 base::Unretained(static_cast<MockWebRtcVideoTrack*>(
                     remote_stream->GetVideoTracks()[0].get()))));
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded,
            video_tracks[0].Source().GetReadyState());
}

TEST_F(RTCPeerConnectionHandlerTest, RemoveAndAddAudioTrackFromRemoteStream) {
  std::string remote_stream_label("remote_stream");
  base::RunLoop run_loop;

  // Grab the added media stream when it's been successfully added to the PC.
  blink::WebMediaStream webkit_stream;
  EXPECT_CALL(*mock_client_.get(),
              DidAddRemoteStream(testing::Property(
                  &blink::WebMediaStream::Id,
                  blink::WebString::FromASCII(remote_stream_label))))
      .WillOnce(DoAll(SaveArg<0>(&webkit_stream),
                      ExitMessageLoop(&message_loop_, run_loop.QuitClosure())));

  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));
  InvokeOnAddStream(remote_stream);
  run_loop.Run();

  {
    // Test in a small scope so that  |audio_tracks| don't hold on to destroyed
    // source later.
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    webkit_stream.AudioTracks(audio_tracks);
    EXPECT_EQ(1u, audio_tracks.size());
  }

  // Remove the Webrtc audio track from the Webrtc MediaStream.
  scoped_refptr<webrtc::AudioTrackInterface> webrtc_track =
      remote_stream->GetAudioTracks()[0].get();
  InvokeRemoveTrack(remote_stream, webrtc_track.get());

  {
    blink::WebVector<blink::WebMediaStreamTrack> modified_audio_tracks1;
    webkit_stream.AudioTracks(modified_audio_tracks1);
    EXPECT_EQ(0u, modified_audio_tracks1.size());
  }

  blink::WebHeap::CollectGarbageForTesting();

  // Add the WebRtc audio track again.
  InvokeAddTrack(remote_stream, webrtc_track.get());
  blink::WebVector<blink::WebMediaStreamTrack> modified_audio_tracks2;
  webkit_stream.AudioTracks(modified_audio_tracks2);
  EXPECT_EQ(1u, modified_audio_tracks2.size());
}

TEST_F(RTCPeerConnectionHandlerTest, RemoveAndAddVideoTrackFromRemoteStream) {
  std::string remote_stream_label("remote_stream");
  base::RunLoop run_loop;

  // Grab the added media stream when it's been successfully added to the PC.
  blink::WebMediaStream webkit_stream;
  EXPECT_CALL(*mock_client_.get(),
              DidAddRemoteStream(testing::Property(
                  &blink::WebMediaStream::Id,
                  blink::WebString::FromASCII(remote_stream_label))))
      .WillOnce(DoAll(SaveArg<0>(&webkit_stream),
                      ExitMessageLoop(&message_loop_, run_loop.QuitClosure())));

  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));
  InvokeOnAddStream(remote_stream);
  run_loop.Run();

  {
    // Test in a small scope so that  |video_tracks| don't hold on to destroyed
    // source later.
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    webkit_stream.VideoTracks(video_tracks);
    EXPECT_EQ(1u, video_tracks.size());
  }

  // Remove the Webrtc video track from the Webrtc MediaStream.
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_track =
      remote_stream->GetVideoTracks()[0].get();
  InvokeRemoveTrack(remote_stream, webrtc_track.get());
  RunMessageLoopsUntilIdle();
  {
    blink::WebVector<blink::WebMediaStreamTrack> modified_video_tracks1;
    webkit_stream.VideoTracks(modified_video_tracks1);
    EXPECT_EQ(0u, modified_video_tracks1.size());
  }

  blink::WebHeap::CollectGarbageForTesting();

  // Add the WebRtc video track again.
  InvokeAddTrack(remote_stream, webrtc_track.get());
  RunMessageLoopsUntilIdle();
  blink::WebVector<blink::WebMediaStreamTrack> modified_video_tracks2;
  webkit_stream.VideoTracks(modified_video_tracks2);
  EXPECT_EQ(1u, modified_video_tracks2.size());
}

TEST_F(RTCPeerConnectionHandlerTest, RemoveAndAddTracksFromRemoteStream) {
  std::string remote_stream_label("remote_stream");
  base::RunLoop run_loop;

  // Grab the added media stream when it's been successfully added to the PC.
  blink::WebMediaStream webkit_stream;
  EXPECT_CALL(*mock_client_.get(),
              DidAddRemoteStream(testing::Property(
                  &blink::WebMediaStream::Id,
                  blink::WebString::FromASCII(remote_stream_label))))
      .WillOnce(DoAll(SaveArg<0>(&webkit_stream),
                      ExitMessageLoop(&message_loop_, run_loop.QuitClosure())));

  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));
  InvokeOnAddStream(remote_stream);
  run_loop.Run();

  {
    // Test in a small scope so that  |audio_tracks| don't hold on to destroyed
    // source later.
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    webkit_stream.AudioTracks(audio_tracks);
    EXPECT_EQ(1u, audio_tracks.size());
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    webkit_stream.VideoTracks(video_tracks);
    EXPECT_EQ(1u, video_tracks.size());
  }

  // Remove the Webrtc tracks from the MediaStream.
  auto audio_track = remote_stream->GetAudioTracks()[0];
  InvokeRemoveTrack(remote_stream, audio_track.get());
  auto video_track = remote_stream->GetVideoTracks()[0];
  InvokeRemoveTrack(remote_stream, video_track.get());
  RunMessageLoopsUntilIdle();

  {
    blink::WebVector<blink::WebMediaStreamTrack> modified_audio_tracks;
    webkit_stream.AudioTracks(modified_audio_tracks);
    EXPECT_EQ(0u, modified_audio_tracks.size());
    blink::WebVector<blink::WebMediaStreamTrack> modified_video_tracks;
    webkit_stream.VideoTracks(modified_video_tracks);
    EXPECT_EQ(0u, modified_video_tracks.size());
  }

  blink::WebHeap::CollectGarbageForTesting();

  // Add the tracks again.
  InvokeAddTrack(remote_stream, audio_track.get());
  InvokeAddTrack(remote_stream, video_track.get());

  blink::WebHeap::CollectGarbageForTesting();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  webkit_stream.AudioTracks(audio_tracks);
  EXPECT_EQ(1u, audio_tracks.size());
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  webkit_stream.VideoTracks(video_tracks);
  EXPECT_EQ(1u, video_tracks.size());
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceCandidate) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(), _,
                                   PeerConnectionTracker::SOURCE_LOCAL, true));
  EXPECT_CALL(*mock_client_.get(), DidGenerateICECandidate(_));

  std::unique_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory_->CreateIceCandidate("sdpMid", 1, kDummySdp));
  pc_handler_->observer()->OnIceCandidate(native_candidate.get());
  RunMessageLoopsUntilIdle();
  EXPECT_EQ("sdpMid", mock_client_->candidate_mid());
  EXPECT_EQ(1, mock_client_->candidate_mlineindex());
  EXPECT_EQ(kDummySdp, mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnRenegotiationNeeded) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackOnRenegotiationNeeded(pc_handler_.get()));
  EXPECT_CALL(*mock_client_.get(), NegotiationNeeded());
  pc_handler_->observer()->OnRenegotiationNeeded();
}

TEST_F(RTCPeerConnectionHandlerTest, CreateDataChannel) {
  blink::WebString label = "d1";
  EXPECT_CALL(*mock_tracker_.get(),
              TrackCreateDataChannel(pc_handler_.get(),
                                     testing::NotNull(),
                                     PeerConnectionTracker::SOURCE_LOCAL));
  std::unique_ptr<blink::WebRTCDataChannelHandler> channel(
      pc_handler_->CreateDataChannel("d1", blink::WebRTCDataChannelInit()));
  EXPECT_TRUE(channel.get() != NULL);
  EXPECT_EQ(label, channel->Label());
  channel->SetClient(nullptr);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateDtmfSender) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(CreateLocalMediaStream(stream_label));
  blink::WebMediaConstraints constraints;
  pc_handler_->AddStream(local_stream, constraints);

  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  local_stream.VideoTracks(tracks);

  ASSERT_LE(1ul, tracks.size());
  EXPECT_FALSE(pc_handler_->CreateDTMFSender(tracks[0]));

  local_stream.AudioTracks(tracks);
  ASSERT_LE(1ul, tracks.size());

  EXPECT_CALL(*mock_tracker_.get(),
              TrackCreateDTMFSender(pc_handler_.get(),
                                    testing::Ref(tracks[0])));

  std::unique_ptr<blink::WebRTCDTMFSenderHandler> sender(
      pc_handler_->CreateDTMFSender(tracks[0]));
  EXPECT_TRUE(sender.get());

  StopAllTracks(local_stream);
}

}  // namespace content
