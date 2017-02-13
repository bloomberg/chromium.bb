// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/user_media_client_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/common/media/media_devices.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using testing::_;

namespace content {

blink::WebMediaConstraints CreateDefaultConstraints() {
  MockConstraintFactory factory;
  factory.AddAdvanced();
  return factory.CreateWebMediaConstraints();
}

blink::WebMediaConstraints CreateDeviceConstraints(
    const char* basic_exact_value,
    const char* basic_ideal_value = nullptr,
    const char* advanced_exact_value = nullptr) {
  MockConstraintFactory factory;
  blink::WebMediaTrackConstraintSet basic;
  if (basic_exact_value) {
    factory.basic().deviceId.setExact(
        blink::WebString::fromUTF8(basic_exact_value));
  }
  if (basic_ideal_value) {
    blink::WebString value = blink::WebString::fromUTF8(basic_ideal_value);
    factory.basic().deviceId.setIdeal(
        blink::WebVector<blink::WebString>(&value, 1));
  }

  auto& advanced = factory.AddAdvanced();
  if (advanced_exact_value) {
    blink::WebString value = blink::WebString::fromUTF8(advanced_exact_value);
    advanced.deviceId.setExact(value);
  }

  return factory.CreateWebMediaConstraints();
}

blink::WebMediaConstraints CreateFacingModeConstraints(
    const char* basic_exact_value,
    const char* basic_ideal_value = nullptr,
    const char* advanced_exact_value = nullptr) {
  MockConstraintFactory factory;
  if (basic_exact_value) {
    factory.basic().facingMode.setExact(
        blink::WebString::fromUTF8(basic_exact_value));
  }
  if (basic_ideal_value) {
    blink::WebString value = blink::WebString::fromUTF8(basic_ideal_value);
    factory.basic().deviceId.setIdeal(
        blink::WebVector<blink::WebString>(&value, 1));
  }

  auto& advanced = factory.AddAdvanced();
  if (advanced_exact_value) {
    blink::WebString value = blink::WebString::fromUTF8(advanced_exact_value);
    advanced.deviceId.setExact(value);
  }

  return factory.CreateWebMediaConstraints();
}

class MockMediaStreamVideoCapturerSource : public MockMediaStreamVideoSource {
 public:
  MockMediaStreamVideoCapturerSource(
      const StreamDeviceInfo& device,
      const SourceStoppedCallback& stop_callback,
      PeerConnectionDependencyFactory* factory)
  : MockMediaStreamVideoSource(false) {
    SetDeviceInfo(device);
    SetStopCallback(stop_callback);
  }
};

const char kInvalidDeviceId[] = "invalid";
const char kFakeAudioInputDeviceId1[] = "fake_audio_input 1";
const char kFakeAudioInputDeviceId2[] = "fake_audio_input 2";
const char kFakeVideoInputDeviceId1[] = "fake_video_input 1";
const char kFakeVideoInputDeviceId2[] = "fake_video_input 2";
const char kFakeAudioOutputDeviceId1[] = "fake_audio_output 1";

class MockMediaDevicesDispatcherHost
    : public ::mojom::MediaDevicesDispatcherHost {
 public:
  MockMediaDevicesDispatcherHost() {}
  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        const url::Origin& security_origin,
                        const EnumerateDevicesCallback& callback) override {
    std::vector<std::vector<MediaDeviceInfo>> result(NUM_MEDIA_DEVICE_TYPES);
    if (request_audio_input) {
      result[MEDIA_DEVICE_TYPE_AUDIO_INPUT].push_back(MediaDeviceInfo(
          kFakeAudioInputDeviceId1, "Fake Audio Input 1", "fake_group 1"));
      result[MEDIA_DEVICE_TYPE_AUDIO_INPUT].push_back(MediaDeviceInfo(
          kFakeAudioInputDeviceId2, "Fake Audio Input 2", "fake_group 2"));
    }
    if (request_video_input) {
      result[MEDIA_DEVICE_TYPE_VIDEO_INPUT].push_back(
          MediaDeviceInfo(kFakeVideoInputDeviceId1, "Fake Video Input 1", ""));
      result[MEDIA_DEVICE_TYPE_VIDEO_INPUT].push_back(
          MediaDeviceInfo(kFakeVideoInputDeviceId2, "Fake Video Input 2", ""));
    }
    if (request_audio_output) {
      result[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].push_back(MediaDeviceInfo(
          kFakeAudioOutputDeviceId1, "Fake Audio Input 1", "fake_group 1"));
    }
    callback.Run(result);
  }

  void GetVideoInputCapabilities(
      const url::Origin& security_origin,
      const GetVideoInputCapabilitiesCallback& client_callback) override {
    ::mojom::VideoInputDeviceCapabilitiesPtr device =
        ::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kFakeVideoInputDeviceId1;
    device->facing_mode = ::mojom::FacingMode::USER;
    device->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_I420));
    std::vector<::mojom::VideoInputDeviceCapabilitiesPtr> result;
    result.push_back(std::move(device));

    device = ::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kFakeVideoInputDeviceId2;
    device->facing_mode = ::mojom::FacingMode::ENVIRONMENT;
    device->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_I420));
    result.push_back(std::move(device));

    client_callback.Run(std::move(result));
  }

  MOCK_METHOD3(SubscribeDeviceChangeNotifications,
               void(MediaDeviceType type,
                    uint32_t subscription_id,
                    const url::Origin& security_origin));
  MOCK_METHOD2(UnsubscribeDeviceChangeNotifications,
               void(MediaDeviceType type, uint32_t subscription_id));
};

class UserMediaClientImplUnderTest : public UserMediaClientImpl {
 public:
  enum RequestState {
    REQUEST_NOT_STARTED,
    REQUEST_NOT_COMPLETE,
    REQUEST_SUCCEEDED,
    REQUEST_FAILED,
  };

  UserMediaClientImplUnderTest(
      PeerConnectionDependencyFactory* dependency_factory,
      std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher)
      : UserMediaClientImpl(nullptr,
                            dependency_factory,
                            std::move(media_stream_dispatcher),
                            base::ThreadTaskRunnerHandle::Get()),
        state_(REQUEST_NOT_STARTED),
        result_(NUM_MEDIA_REQUEST_RESULTS),
        result_name_(""),
        factory_(dependency_factory),
        create_source_that_fails_(false),
        video_source_(nullptr) {}

  void RequestUserMediaForTest(
      const blink::WebUserMediaRequest& user_media_request) {
    state_ = REQUEST_NOT_COMPLETE;
    requestUserMedia(user_media_request);
    base::RunLoop().RunUntilIdle();
  }

  void RequestUserMediaForTest() {
    blink::WebUserMediaRequest user_media_request =
        blink::WebUserMediaRequest::createForTesting(
            CreateDefaultConstraints(), CreateDefaultConstraints());
    RequestUserMediaForTest(user_media_request);
  }

  void RequestMediaDevicesForTest() {
    blink::WebMediaDevicesRequest media_devices_request;
    state_ = REQUEST_NOT_COMPLETE;
    requestMediaDevices(media_devices_request);
  }

  void GetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest request_info) override {
    last_generated_stream_ = stream;
    state_ = REQUEST_SUCCEEDED;
  }

  void GetUserMediaRequestFailed(
      blink::WebUserMediaRequest request_info,
      content::MediaStreamRequestResult result,
      const blink::WebString& result_name) override {
    last_generated_stream_.reset();
    state_ = REQUEST_FAILED;
    result_ = result;
    result_name_ = result_name;
  }

  void EnumerateDevicesSucceded(
      blink::WebMediaDevicesRequest* request,
      blink::WebVector<blink::WebMediaDeviceInfo>& devices) override {
    state_ = REQUEST_SUCCEEDED;
    last_devices_ = devices;
  }

  void SetCreateSourceThatFails(bool should_fail) {
    create_source_that_fails_ = should_fail;
  }

  static void SignalSourceReady(
      const MediaStreamSource::ConstraintsCallback& source_ready,
      MediaStreamSource* source) {
    source_ready.Run(source, MEDIA_DEVICE_OK, "");
  }

  MediaStreamAudioSource* CreateAudioSource(
      const StreamDeviceInfo& device,
      const blink::WebMediaConstraints& constraints,
      const MediaStreamSource::ConstraintsCallback& source_ready) override {
    MediaStreamAudioSource* source;
    if (create_source_that_fails_) {
      class FailedAtLifeAudioSource : public MediaStreamAudioSource {
       public:
        FailedAtLifeAudioSource() : MediaStreamAudioSource(true) {}
        ~FailedAtLifeAudioSource() override {}
       protected:
        bool EnsureSourceIsStarted() override {
          return false;
        }
      };
      source = new FailedAtLifeAudioSource();
    } else {
      source = new MediaStreamAudioSource(true);
    }
    source->SetDeviceInfo(device);

    if (!create_source_that_fails_) {
      // RunUntilIdle is required for this task to complete.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&UserMediaClientImplUnderTest::SignalSourceReady,
                     source_ready, source));
    }

    return source;
  }

  MediaStreamVideoSource* CreateVideoSource(
      const StreamDeviceInfo& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback) override {
    video_source_ = new MockMediaStreamVideoCapturerSource(device,
                                                           stop_callback,
                                                           factory_);
    return video_source_;
  }

  const blink::WebMediaStream& last_generated_stream() {
    return last_generated_stream_;
  }

  const blink::WebVector<blink::WebMediaDeviceInfo>& last_devices() {
    return last_devices_;
  }

  void ClearLastGeneratedStream() {
    last_generated_stream_.reset();
  }

  MockMediaStreamVideoCapturerSource* last_created_video_source() const {
    return video_source_;
  }

  RequestState request_state() const { return state_; }
  content::MediaStreamRequestResult error_reason() const { return result_; }
  blink::WebString error_name() const { return result_name_; }

  // Access to the request queue for testing.
  bool UserMediaRequestHasAutomaticDeviceSelection(int request_id) {
    auto* request = FindUserMediaRequestInfo(request_id);
    EXPECT_TRUE(request != nullptr);
    return request && request->enable_automatic_output_device_selection;
  }

  void DeleteRequest(int request_id) {
    auto* request = FindUserMediaRequestInfo(request_id);
    DeleteUserMediaRequestInfo(request);
  }

 private:
  blink::WebMediaStream last_generated_stream_;
  RequestState state_;
  content::MediaStreamRequestResult result_;
  blink::WebString result_name_;
  blink::WebVector<blink::WebMediaDeviceInfo> last_devices_;
  PeerConnectionDependencyFactory* factory_;
  bool create_source_that_fails_;
  MockMediaStreamVideoCapturerSource* video_source_;
};

class UserMediaClientImplTest : public ::testing::Test {
 public:
  UserMediaClientImplTest()
      : binding_user_media(&media_devices_dispatcher_),
        binding_event_dispatcher_(&media_devices_dispatcher_) {}

  void SetUp() override {
    // Create our test object.
    child_process_.reset(new ChildProcess());
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    ms_dispatcher_ = new MockMediaStreamDispatcher();
    user_media_client_impl_.reset(new UserMediaClientImplUnderTest(
        dependency_factory_.get(),
        std::unique_ptr<MediaStreamDispatcher>(ms_dispatcher_)));
    user_media_client_impl_->SetMediaDevicesDispatcherForTesting(
        binding_user_media.CreateInterfacePtrAndBind());
    base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
        MediaDevicesEventDispatcher::GetForRenderFrame(nullptr);
    event_dispatcher->SetMediaDevicesDispatcherForTesting(
        binding_event_dispatcher_.CreateInterfacePtrAndBind());
  }

  void TearDown() override {
    MediaDevicesEventDispatcher::GetForRenderFrame(nullptr)->OnDestruct();
    user_media_client_impl_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  void LoadNewDocumentInFrame() {
    user_media_client_impl_->WillCommitProvisionalLoad();
  }

  blink::WebMediaStream RequestLocalMediaStream() {
    user_media_client_impl_->RequestUserMediaForTest();
    FakeMediaStreamDispatcherRequestUserMediaComplete();
    StartMockedVideoSource();

    EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_SUCCEEDED,
              user_media_client_impl_->request_state());

    blink::WebMediaStream desc =
        user_media_client_impl_->last_generated_stream();
    content::MediaStream* native_stream =
        content::MediaStream::GetMediaStream(desc);
    if (!native_stream) {
      ADD_FAILURE();
      return desc;
    }

    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    desc.audioTracks(audio_tracks);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    desc.videoTracks(video_tracks);

    EXPECT_EQ(1u, audio_tracks.size());
    EXPECT_EQ(1u, video_tracks.size());
    EXPECT_NE(audio_tracks[0].id(), video_tracks[0].id());
    return desc;
  }

  void FakeMediaStreamDispatcherRequestUserMediaComplete() {
    // Audio request ID is used as the shared request ID.
    user_media_client_impl_->OnStreamGenerated(
        ms_dispatcher_->audio_input_request_id(),
        ms_dispatcher_->stream_label(), ms_dispatcher_->audio_input_array(),
        ms_dispatcher_->video_array());
    base::RunLoop().RunUntilIdle();
  }

  void StartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        user_media_client_impl_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->StartMockedSource();
  }

  void FailToStartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        user_media_client_impl_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->FailToStartMockedSource();
    blink::WebHeap::collectGarbageForTesting();
  }

  bool AudioRequestHasAutomaticDeviceSelection(
      const blink::WebMediaConstraints& audio_constraints) {
    blink::WebMediaConstraints null_constraints;
    blink::WebUserMediaRequest request =
        blink::WebUserMediaRequest::createForTesting(audio_constraints,
                                                     null_constraints);
    user_media_client_impl_->RequestUserMediaForTest(request);
    bool result =
        user_media_client_impl_->UserMediaRequestHasAutomaticDeviceSelection(
            ms_dispatcher_->audio_input_request_id());
    user_media_client_impl_->DeleteRequest(
        ms_dispatcher_->audio_input_request_id());
    return result;
  }

  void TestValidRequestWithConstraints(
      const blink::WebMediaConstraints& audio_constraints,
      const blink::WebMediaConstraints& video_constraints,
      const std::string& expected_audio_device_id,
      const std::string& expected_video_device_id) {
    DCHECK(!audio_constraints.isNull());
    DCHECK(!video_constraints.isNull());
    blink::WebUserMediaRequest request =
        blink::WebUserMediaRequest::createForTesting(audio_constraints,
                                                     video_constraints);
    user_media_client_impl_->RequestUserMediaForTest(request);
    FakeMediaStreamDispatcherRequestUserMediaComplete();
    StartMockedVideoSource();

    EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_SUCCEEDED,
              user_media_client_impl_->request_state());
    EXPECT_EQ(1U, ms_dispatcher_->audio_input_array().size());
    EXPECT_EQ(1U, ms_dispatcher_->video_array().size());
    // MockMediaStreamDispatcher appends the session ID to its internal device
    // IDs.
    EXPECT_EQ(std::string(expected_audio_device_id) + "0",
              ms_dispatcher_->audio_input_array()[0].device.id);
    EXPECT_EQ(std::string(expected_video_device_id) + "0",
              ms_dispatcher_->video_array()[0].device.id);
  }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<ChildProcess> child_process_;
  MockMediaStreamDispatcher* ms_dispatcher_;  // Owned by |used_media_impl_|.
  MockMediaDevicesDispatcherHost media_devices_dispatcher_;
  mojo::Binding<::mojom::MediaDevicesDispatcherHost> binding_user_media;
  mojo::Binding<::mojom::MediaDevicesDispatcherHost> binding_event_dispatcher_;

  std::unique_ptr<UserMediaClientImplUnderTest> user_media_client_impl_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
};

TEST_F(UserMediaClientImplTest, GenerateMediaStream) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
}

// Test that the same source object is used if two MediaStreams are generated
// using the same source.
TEST_F(UserMediaClientImplTest, GenerateTwoMediaStreamsWithSameSource) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks;
  desc1.videoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.videoTracks(desc2_video_tracks);
  EXPECT_EQ(desc1_video_tracks[0].source().id(),
            desc2_video_tracks[0].source().id());

  EXPECT_EQ(desc1_video_tracks[0].source().getExtraData(),
            desc2_video_tracks[0].source().getExtraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.audioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.audioTracks(desc2_audio_tracks);
  EXPECT_EQ(desc1_audio_tracks[0].source().id(),
            desc2_audio_tracks[0].source().id());

  EXPECT_EQ(MediaStreamAudioSource::From(desc1_audio_tracks[0].source()),
            MediaStreamAudioSource::From(desc2_audio_tracks[0].source()));
}

// Test that the same source object is not used if two MediaStreams are
// generated using different sources.
TEST_F(UserMediaClientImplTest, GenerateTwoMediaStreamsWithDifferentSources) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  // Make sure another device is selected (another |session_id|) in  the next
  // gUM request.
  ms_dispatcher_->IncrementSessionId();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks;
  desc1.videoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.videoTracks(desc2_video_tracks);
  EXPECT_NE(desc1_video_tracks[0].source().id(),
            desc2_video_tracks[0].source().id());

  EXPECT_NE(desc1_video_tracks[0].source().getExtraData(),
            desc2_video_tracks[0].source().getExtraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.audioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.audioTracks(desc2_audio_tracks);
  EXPECT_NE(desc1_audio_tracks[0].source().id(),
            desc2_audio_tracks[0].source().id());

  EXPECT_NE(MediaStreamAudioSource::From(desc1_audio_tracks[0].source()),
            MediaStreamAudioSource::From(desc2_audio_tracks[0].source()));
}

TEST_F(UserMediaClientImplTest, StopLocalTracks) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.audioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.videoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test that a source is not stopped even if the tracks in a
// MediaStream is stopped if there are two MediaStreams with tracks using the
// same device. The source is stopped
// if there are no more MediaStream tracks using the device.
TEST_F(UserMediaClientImplTest, StopLocalTracksWhenTwoStreamUseSameDevices) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks1;
  desc1.audioTracks(audio_tracks1);
  MediaStreamTrack* audio_track1 = MediaStreamTrack::GetTrack(audio_tracks1[0]);
  audio_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks2;
  desc2.audioTracks(audio_tracks2);
  MediaStreamTrack* audio_track2 = MediaStreamTrack::GetTrack(audio_tracks2[0]);
  audio_track2->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks1;
  desc1.videoTracks(video_tracks1);
  MediaStreamTrack* video_track1 = MediaStreamTrack::GetTrack(video_tracks1[0]);
  video_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks2;
  desc2.videoTracks(video_tracks2);
  MediaStreamTrack* video_track2 = MediaStreamTrack::GetTrack(video_tracks2[0]);
  video_track2->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

TEST_F(UserMediaClientImplTest, StopSourceWhenMediaStreamGoesOutOfScope) {
  // Generate a stream with both audio and video.
  RequestLocalMediaStream();
  // Makes sure the test itself don't hold a reference to the created
  // MediaStream.
  user_media_client_impl_->ClearLastGeneratedStream();
  blink::WebHeap::collectAllGarbageForTesting();

  // Expect the sources to be stopped when the MediaStream goes out of scope.
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// Test that the MediaStreams are deleted if a new document is loaded in the
// frame.
TEST_F(UserMediaClientImplTest, LoadNewDocumentInFrame) {
  // Test a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();
  LoadNewDocumentInFrame();
  blink::WebHeap::collectAllGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if a video source to a MediaSteam fails to start.
TEST_F(UserMediaClientImplTest, MediaVideoSourceFailToStart) {
  user_media_client_impl_->RequestUserMediaForTest();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  FailToStartMockedVideoSource();
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_FAILED,
            user_media_client_impl_->request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE,
            user_media_client_impl_->error_reason());
  blink::WebHeap::collectAllGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if an audio source fail to initialize.
TEST_F(UserMediaClientImplTest, MediaAudioSourceFailToInitialize) {
  user_media_client_impl_->SetCreateSourceThatFails(true);
  user_media_client_impl_->RequestUserMediaForTest();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  StartMockedVideoSource();
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_FAILED,
            user_media_client_impl_->request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE,
            user_media_client_impl_->error_reason());
  blink::WebHeap::collectAllGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

// This test what happens if UserMediaClientImpl is deleted before a source has
// started.
TEST_F(UserMediaClientImplTest, MediaStreamImplShutDown) {
  user_media_client_impl_->RequestUserMediaForTest();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_NOT_COMPLETE,
            user_media_client_impl_->request_state());
  user_media_client_impl_.reset();
}

// This test what happens if a new document is loaded in the frame while the
// MediaStream is being generated by the MediaStreamDispatcher.
TEST_F(UserMediaClientImplTest, ReloadFrameWhileGeneratingStream) {
  user_media_client_impl_->RequestUserMediaForTest();
  LoadNewDocumentInFrame();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_NOT_COMPLETE,
            user_media_client_impl_->request_state());
}

// This test what happens if a newdocument is loaded in the frame while the
// sources are being started.
TEST_F(UserMediaClientImplTest, ReloadFrameWhileGeneratingSources) {
  user_media_client_impl_->RequestUserMediaForTest();
  FakeMediaStreamDispatcherRequestUserMediaComplete();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  LoadNewDocumentInFrame();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_NOT_COMPLETE,
            user_media_client_impl_->request_state());
}

// This test what happens if stop is called on a track after the frame has
// been reloaded.
TEST_F(UserMediaClientImplTest, StopTrackAfterReload) {
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  LoadNewDocumentInFrame();
  blink::WebHeap::collectAllGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.audioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.videoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
}

TEST_F(UserMediaClientImplTest, EnumerateMediaDevices) {
  user_media_client_impl_->RequestMediaDevicesForTest();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_SUCCEEDED,
            user_media_client_impl_->request_state());
  EXPECT_EQ(static_cast<size_t>(5),
            user_media_client_impl_->last_devices().size());

  // Audio input device with matched output ID.
  const blink::WebMediaDeviceInfo* device =
      &user_media_client_impl_->last_devices()[0];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_FALSE(device->groupId().isEmpty());

  // Audio input device without matched output ID.
  device = &user_media_client_impl_->last_devices()[1];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_FALSE(device->groupId().isEmpty());

  // Video input devices.
  device = &user_media_client_impl_->last_devices()[2];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_TRUE(device->groupId().isEmpty());

  device = &user_media_client_impl_->last_devices()[3];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_TRUE(device->groupId().isEmpty());

  // Audio output device.
  device = &user_media_client_impl_->last_devices()[4];
  EXPECT_FALSE(device->deviceId().isEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
            device->kind());
  EXPECT_FALSE(device->label().isEmpty());
  EXPECT_FALSE(device->groupId().isEmpty());

  // Verfify group IDs.
  EXPECT_TRUE(user_media_client_impl_->last_devices()[0].groupId().equals(
      user_media_client_impl_->last_devices()[4].groupId()));
  EXPECT_FALSE(user_media_client_impl_->last_devices()[1].groupId().equals(
      user_media_client_impl_->last_devices()[4].groupId()));
}

TEST_F(UserMediaClientImplTest, RenderToAssociatedSinkConstraint) {
  // For a null UserMediaRequest (no audio requested), we expect false.
  user_media_client_impl_->RequestUserMediaForTest();
  EXPECT_FALSE(
      user_media_client_impl_->UserMediaRequestHasAutomaticDeviceSelection(
          ms_dispatcher_->audio_input_request_id()));
  user_media_client_impl_->DeleteRequest(
      ms_dispatcher_->audio_input_request_id());

  // If audio is requested, but no constraint, it should be true.
  // Currently we expect it to be false due to a suspected bug in the
  // device-matching code causing issues with some sound adapters.
  // See crbug.com/604523
  MockConstraintFactory factory;
  blink::WebMediaConstraints audio_constraints =
      factory.CreateWebMediaConstraints();
  EXPECT_FALSE(AudioRequestHasAutomaticDeviceSelection(
      factory.CreateWebMediaConstraints()));

  // If the constraint is present, it should dictate the result.
  factory.Reset();
  factory.AddAdvanced().renderToAssociatedSink.setExact(true);
  EXPECT_TRUE(AudioRequestHasAutomaticDeviceSelection(
      factory.CreateWebMediaConstraints()));

  factory.Reset();
  factory.AddAdvanced().renderToAssociatedSink.setExact(false);
  EXPECT_FALSE(AudioRequestHasAutomaticDeviceSelection(
      factory.CreateWebMediaConstraints()));

  factory.Reset();
  factory.basic().renderToAssociatedSink.setExact(false);
  EXPECT_FALSE(AudioRequestHasAutomaticDeviceSelection(
      factory.CreateWebMediaConstraints()));
}

TEST_F(UserMediaClientImplTest, ObserveMediaDeviceChanges) {
  EXPECT_CALL(
      media_devices_dispatcher_,
      SubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_AUDIO_INPUT, _, _));
  EXPECT_CALL(
      media_devices_dispatcher_,
      SubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_VIDEO_INPUT, _, _));
  EXPECT_CALL(
      media_devices_dispatcher_,
      SubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, _, _));
  user_media_client_impl_->setMediaDeviceChangeObserver(
      blink::WebMediaDeviceChangeObserver(true));
  base::RunLoop().RunUntilIdle();

  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
      MediaDevicesEventDispatcher::GetForRenderFrame(nullptr);
  event_dispatcher->DispatchDevicesChangedEvent(MEDIA_DEVICE_TYPE_AUDIO_INPUT,
                                                MediaDeviceInfoArray());
  event_dispatcher->DispatchDevicesChangedEvent(MEDIA_DEVICE_TYPE_VIDEO_INPUT,
                                                MediaDeviceInfoArray());
  event_dispatcher->DispatchDevicesChangedEvent(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT,
                                                MediaDeviceInfoArray());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(media_devices_dispatcher_, UnsubscribeDeviceChangeNotifications(
                                             MEDIA_DEVICE_TYPE_AUDIO_INPUT, _));
  EXPECT_CALL(media_devices_dispatcher_, UnsubscribeDeviceChangeNotifications(
                                             MEDIA_DEVICE_TYPE_VIDEO_INPUT, _));
  EXPECT_CALL(
      media_devices_dispatcher_,
      UnsubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, _));

  user_media_client_impl_->setMediaDeviceChangeObserver(
      blink::WebMediaDeviceChangeObserver());
  base::RunLoop().RunUntilIdle();
}

// This test what happens if the audio stream has same id with video stream.
TEST_F(UserMediaClientImplTest, AudioVideoWithSameId) {
  ms_dispatcher_->TestSameId();

  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();

  // Remove video track. This should trigger
  // UserMediaClientImpl::OnLocalSourceStopped, and has video track to be
  // removed from its |local_sources_|.
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.videoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());

  // Now we load a new document in the web frame. If in the above Stop() call,
  // UserMediaClientImpl accidentally removed audio track, then video track will
  // be removed again here, which is incorrect.
  LoadNewDocumentInFrame();
  blink::WebHeap::collectAllGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryInvalidAudioDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kInvalidDeviceId);
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::createForTesting(
          audio_constraints, blink::WebMediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_FAILED,
            user_media_client_impl_->request_state());
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryInvalidVideoDeviceId) {
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(kInvalidDeviceId);
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::createForTesting(blink::WebMediaConstraints(),
                                                   video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_FAILED,
            user_media_client_impl_->request_state());
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryValidDeviceIds) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(kFakeVideoInputDeviceId1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithBasicIdealValidDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(nullptr, kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(nullptr, kFakeVideoInputDeviceId1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithAdvancedExactValidDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(nullptr, nullptr, kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints = CreateDeviceConstraints(
      nullptr, nullptr, kFakeVideoInputDeviceId1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithAllOptionalInvalidDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(nullptr, kInvalidDeviceId, kInvalidDeviceId);
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(nullptr, kInvalidDeviceId, kInvalidDeviceId);
  // MockMediaStreamDispatcher uses empty string as default audio device ID.
  // MockMediaDevicesDispatcher uses the first device in the enumeration as
  // default video device ID.
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  std::string(), kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithFacingModeUser) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateFacingModeConstraints("user");
  // kFakeVideoInputDeviceId1 has user facing mode.
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithFacingModeEnvironment) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateFacingModeConstraints("environment");
  // kFakeVideoInputDeviceId2 has environment facing mode.
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId2);
}

}  // namespace content
