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
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_constraints_util_video_content.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/mock_mojo_media_stream_dispatcher_host.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "media/audio/audio_device_description.h"
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
  if (basic_exact_value) {
    factory.basic().device_id.SetExact(
        blink::WebString::FromUTF8(basic_exact_value));
  }
  if (basic_ideal_value) {
    blink::WebString value = blink::WebString::FromUTF8(basic_ideal_value);
    factory.basic().device_id.SetIdeal(
        blink::WebVector<blink::WebString>(&value, 1));
  }

  auto& advanced = factory.AddAdvanced();
  if (advanced_exact_value) {
    blink::WebString value = blink::WebString::FromUTF8(advanced_exact_value);
    advanced.device_id.SetExact(value);
  }

  return factory.CreateWebMediaConstraints();
}

blink::WebMediaConstraints CreateFacingModeConstraints(
    const char* basic_exact_value,
    const char* basic_ideal_value = nullptr,
    const char* advanced_exact_value = nullptr) {
  MockConstraintFactory factory;
  if (basic_exact_value) {
    factory.basic().facing_mode.SetExact(
        blink::WebString::FromUTF8(basic_exact_value));
  }
  if (basic_ideal_value) {
    blink::WebString value = blink::WebString::FromUTF8(basic_ideal_value);
    factory.basic().device_id.SetIdeal(
        blink::WebVector<blink::WebString>(&value, 1));
  }

  auto& advanced = factory.AddAdvanced();
  if (advanced_exact_value) {
    blink::WebString value = blink::WebString::FromUTF8(advanced_exact_value);
    advanced.device_id.SetExact(value);
  }

  return factory.CreateWebMediaConstraints();
}

class MockMediaStreamVideoCapturerSource : public MockMediaStreamVideoSource {
 public:
  MockMediaStreamVideoCapturerSource(const MediaStreamDevice& device,
                                     const SourceStoppedCallback& stop_callback,
                                     PeerConnectionDependencyFactory* factory)
      : MockMediaStreamVideoSource() {
    SetDevice(device);
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
                        EnumerateDevicesCallback callback) override {
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
    std::move(callback).Run(result);
  }

  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override {
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

    std::move(client_callback).Run(std::move(result));
  }

  void GetAudioInputCapabilities(
      GetAudioInputCapabilitiesCallback client_callback) override {
    std::vector<::mojom::AudioInputDeviceCapabilitiesPtr> result;
    ::mojom::AudioInputDeviceCapabilitiesPtr device =
        ::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = media::AudioDeviceDescription::kDefaultDeviceId;
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    device = ::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = kFakeAudioInputDeviceId1;
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    device = ::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = kFakeAudioInputDeviceId2;
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    std::move(client_callback).Run(std::move(result));
  }

  media::AudioParameters& AudioParameters() { return audio_parameters_; }

  void ResetAudioParameters() {
    audio_parameters_ = media::AudioParameters::UnavailableDeviceParams();
  }

  MOCK_METHOD2(SubscribeDeviceChangeNotifications,
               void(MediaDeviceType type, uint32_t subscription_id));
  MOCK_METHOD2(UnsubscribeDeviceChangeNotifications,
               void(MediaDeviceType type, uint32_t subscription_id));

 private:
  media::AudioParameters audio_parameters_ =
      media::AudioParameters::UnavailableDeviceParams();
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
    RequestUserMedia(user_media_request);
    base::RunLoop().RunUntilIdle();
  }

  void RequestUserMediaForTest() {
    blink::WebUserMediaRequest user_media_request =
        blink::WebUserMediaRequest::CreateForTesting(
            CreateDefaultConstraints(), CreateDefaultConstraints());
    RequestUserMediaForTest(user_media_request);
  }

  void RequestMediaDevicesForTest() {
    blink::WebMediaDevicesRequest media_devices_request;
    state_ = REQUEST_NOT_COMPLETE;
    RequestMediaDevices(media_devices_request);
  }

  void GetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest request_info) override {
    last_generated_stream_ = stream;
    state_ = REQUEST_SUCCEEDED;
  }

  void GetUserMediaRequestFailed(
      content::MediaStreamRequestResult result,
      const blink::WebString& result_name) override {
    last_generated_stream_.Reset();
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
      const MediaStreamDevice& device,
      const blink::WebMediaConstraints& constraints,
      const MediaStreamSource::ConstraintsCallback& source_ready,
      bool* has_sw_echo_cancellation) override {
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

    source->SetDevice(device);

    if (!create_source_that_fails_) {
      // RunUntilIdle is required for this task to complete.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&UserMediaClientImplUnderTest::SignalSourceReady,
                         source_ready, source));
    }

    *has_sw_echo_cancellation = false;
    return source;
  }

  MediaStreamVideoSource* CreateVideoSource(
      const MediaStreamDevice& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback) override {
    video_source_ =
        new MockMediaStreamVideoCapturerSource(device, stop_callback, factory_);
    return video_source_;
  }

  const blink::WebMediaStream& last_generated_stream() {
    return last_generated_stream_;
  }

  const blink::WebVector<blink::WebMediaDeviceInfo>& last_devices() {
    return last_devices_;
  }

  void ClearLastGeneratedStream() { last_generated_stream_.Reset(); }

  MockMediaStreamVideoCapturerSource* last_created_video_source() const {
    return video_source_;
  }

  RequestState request_state() const { return state_; }
  content::MediaStreamRequestResult error_reason() const { return result_; }
  blink::WebString error_name() const { return result_name_; }

  AudioCaptureSettings AudioSettings() const {
    return AudioCaptureSettingsForTesting();
  }
  VideoCaptureSettings VideoSettings() const {
    return VideoCaptureSettingsForTesting();
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
      : binding_user_media_(&media_devices_dispatcher_),
        binding_event_dispatcher_(&media_devices_dispatcher_) {
  }

  void SetUp() override {
    // Create our test object.
    child_process_.reset(new ChildProcess());
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    ms_dispatcher_ = new MockMediaStreamDispatcher();
    mojom::MediaStreamDispatcherHostPtr dispatcher_host =
        mock_dispatcher_host_.CreateInterfacePtrAndBind();
    ms_dispatcher_->dispatcher_host_ = std::move(dispatcher_host);
    user_media_client_impl_.reset(new UserMediaClientImplUnderTest(
        dependency_factory_.get(),
        std::unique_ptr<MediaStreamDispatcher>(ms_dispatcher_)));
    ::mojom::MediaDevicesDispatcherHostPtr user_media_host_proxy;
    binding_user_media_.Bind(mojo::MakeRequest(&user_media_host_proxy));
    user_media_client_impl_->SetMediaDevicesDispatcherForTesting(
        std::move(user_media_host_proxy));
    base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
        MediaDevicesEventDispatcher::GetForRenderFrame(nullptr);
    ::mojom::MediaDevicesDispatcherHostPtr event_dispatcher_host_proxy;
    binding_event_dispatcher_.Bind(
        mojo::MakeRequest(&event_dispatcher_host_proxy));
    event_dispatcher->SetMediaDevicesDispatcherForTesting(
        std::move(event_dispatcher_host_proxy));
  }

  void TearDown() override {
    MediaDevicesEventDispatcher::GetForRenderFrame(nullptr)->OnDestruct();
    user_media_client_impl_.reset();
    blink::WebHeap::CollectAllGarbageForTesting();
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
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    desc.AudioTracks(audio_tracks);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    desc.VideoTracks(video_tracks);

    EXPECT_EQ(1u, audio_tracks.size());
    EXPECT_EQ(1u, video_tracks.size());
    EXPECT_NE(audio_tracks[0].Id(), video_tracks[0].Id());
    return desc;
  }

  void FakeMediaStreamDispatcherRequestUserMediaComplete() {
    // Audio request ID is used as the shared request ID.
    user_media_client_impl_->OnStreamGenerated(
        ms_dispatcher_->audio_input_request_id(),
        ms_dispatcher_->stream_label(), ms_dispatcher_->audio_devices(),
        ms_dispatcher_->video_devices());
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
    blink::WebHeap::CollectGarbageForTesting();
  }

  void TestValidRequestWithConstraints(
      const blink::WebMediaConstraints& audio_constraints,
      const blink::WebMediaConstraints& video_constraints,
      const std::string& expected_audio_device_id,
      const std::string& expected_video_device_id) {
    DCHECK(!audio_constraints.IsNull());
    DCHECK(!video_constraints.IsNull());
    blink::WebUserMediaRequest request =
        blink::WebUserMediaRequest::CreateForTesting(audio_constraints,
                                                     video_constraints);
    user_media_client_impl_->RequestUserMediaForTest(request);
    FakeMediaStreamDispatcherRequestUserMediaComplete();
    StartMockedVideoSource();

    EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_SUCCEEDED,
              user_media_client_impl_->request_state());
    EXPECT_EQ(1U, ms_dispatcher_->audio_devices().size());
    EXPECT_EQ(1U, ms_dispatcher_->video_devices().size());
    // MockMediaStreamDispatcher appends the session ID to its internal device
    // IDs.
    EXPECT_EQ(std::string(expected_audio_device_id) + "0",
              ms_dispatcher_->audio_devices()[0].id);
    EXPECT_EQ(std::string(expected_video_device_id) + "0",
              ms_dispatcher_->video_devices()[0].id);
  }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<ChildProcess> child_process_;
  MockMediaStreamDispatcher* ms_dispatcher_;  // Owned by |used_media_impl_|.
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  MockMediaDevicesDispatcherHost media_devices_dispatcher_;
  mojo::Binding<::mojom::MediaDevicesDispatcherHost> binding_user_media_;
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
  desc1.VideoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.VideoTracks(desc2_video_tracks);
  EXPECT_EQ(desc1_video_tracks[0].Source().Id(),
            desc2_video_tracks[0].Source().Id());

  EXPECT_EQ(desc1_video_tracks[0].Source().GetExtraData(),
            desc2_video_tracks[0].Source().GetExtraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.AudioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.AudioTracks(desc2_audio_tracks);
  EXPECT_EQ(desc1_audio_tracks[0].Source().Id(),
            desc2_audio_tracks[0].Source().Id());

  EXPECT_EQ(MediaStreamAudioSource::From(desc1_audio_tracks[0].Source()),
            MediaStreamAudioSource::From(desc2_audio_tracks[0].Source()));
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
  desc1.VideoTracks(desc1_video_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks;
  desc2.VideoTracks(desc2_video_tracks);
  EXPECT_NE(desc1_video_tracks[0].Source().Id(),
            desc2_video_tracks[0].Source().Id());

  EXPECT_NE(desc1_video_tracks[0].Source().GetExtraData(),
            desc2_video_tracks[0].Source().GetExtraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks;
  desc1.AudioTracks(desc1_audio_tracks);
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks;
  desc2.AudioTracks(desc2_audio_tracks);
  EXPECT_NE(desc1_audio_tracks[0].Source().Id(),
            desc2_audio_tracks[0].Source().Id());

  EXPECT_NE(MediaStreamAudioSource::From(desc1_audio_tracks[0].Source()),
            MediaStreamAudioSource::From(desc2_audio_tracks[0].Source()));
}

TEST_F(UserMediaClientImplTest, StopLocalTracks) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.AudioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.VideoTracks(video_tracks);
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
  desc1.AudioTracks(audio_tracks1);
  MediaStreamTrack* audio_track1 = MediaStreamTrack::GetTrack(audio_tracks1[0]);
  audio_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks2;
  desc2.AudioTracks(audio_tracks2);
  MediaStreamTrack* audio_track2 = MediaStreamTrack::GetTrack(audio_tracks2[0]);
  audio_track2->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks1;
  desc1.VideoTracks(video_tracks1);
  MediaStreamTrack* video_track1 = MediaStreamTrack::GetTrack(video_tracks1[0]);
  video_track1->Stop();
  EXPECT_EQ(0, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks2;
  desc2.VideoTracks(video_tracks2);
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
  blink::WebHeap::CollectAllGarbageForTesting();

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
  blink::WebHeap::CollectAllGarbageForTesting();
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
  blink::WebHeap::CollectAllGarbageForTesting();
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
  blink::WebHeap::CollectAllGarbageForTesting();
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
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  mixed_desc.AudioTracks(audio_tracks);
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  mixed_desc.VideoTracks(video_tracks);
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
  EXPECT_FALSE(device->DeviceId().IsEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::kMediaDeviceKindAudioInput,
            device->Kind());
  EXPECT_FALSE(device->Label().IsEmpty());
  EXPECT_FALSE(device->GroupId().IsEmpty());

  // Audio input device without matched output ID.
  device = &user_media_client_impl_->last_devices()[1];
  EXPECT_FALSE(device->DeviceId().IsEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::kMediaDeviceKindAudioInput,
            device->Kind());
  EXPECT_FALSE(device->Label().IsEmpty());
  EXPECT_FALSE(device->GroupId().IsEmpty());

  // Video input devices.
  device = &user_media_client_impl_->last_devices()[2];
  EXPECT_FALSE(device->DeviceId().IsEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::kMediaDeviceKindVideoInput,
            device->Kind());
  EXPECT_FALSE(device->Label().IsEmpty());
  EXPECT_TRUE(device->GroupId().IsEmpty());

  device = &user_media_client_impl_->last_devices()[3];
  EXPECT_FALSE(device->DeviceId().IsEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::kMediaDeviceKindVideoInput,
            device->Kind());
  EXPECT_FALSE(device->Label().IsEmpty());
  EXPECT_TRUE(device->GroupId().IsEmpty());

  // Audio output device.
  device = &user_media_client_impl_->last_devices()[4];
  EXPECT_FALSE(device->DeviceId().IsEmpty());
  EXPECT_EQ(blink::WebMediaDeviceInfo::kMediaDeviceKindAudioOutput,
            device->Kind());
  EXPECT_FALSE(device->Label().IsEmpty());
  EXPECT_FALSE(device->GroupId().IsEmpty());

  // Verfify group IDs.
  EXPECT_TRUE(user_media_client_impl_->last_devices()[0].GroupId().Equals(
      user_media_client_impl_->last_devices()[4].GroupId()));
  EXPECT_FALSE(user_media_client_impl_->last_devices()[1].GroupId().Equals(
      user_media_client_impl_->last_devices()[4].GroupId()));
}

TEST_F(UserMediaClientImplTest, DefaultConstraintsPropagate) {
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(CreateDefaultConstraints(),
                                                   CreateDefaultConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_client_impl_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_client_impl_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId,
            audio_capture_settings.device_id());
  EXPECT_FALSE(audio_capture_settings.hotword_enabled());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_TRUE(properties.enable_sw_echo_cancellation);
  EXPECT_FALSE(properties.disable_hw_echo_cancellation);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_TRUE(properties.goog_auto_gain_control);
  // The default value for goog_experimental_echo_cancellation is platform
  // dependent.
  EXPECT_EQ(AudioProcessingProperties().goog_experimental_echo_cancellation,
            properties.goog_experimental_echo_cancellation);
  EXPECT_TRUE(properties.goog_typing_noise_detection);
  EXPECT_TRUE(properties.goog_noise_suppression);
  EXPECT_TRUE(properties.goog_experimental_noise_suppression);
  EXPECT_TRUE(properties.goog_beamforming);
  EXPECT_TRUE(properties.goog_highpass_filter);
  EXPECT_TRUE(properties.goog_experimental_auto_gain_control);
  EXPECT_TRUE(properties.goog_array_geometry.empty());

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(),
            MediaStreamVideoSource::kDefaultWidth);
  EXPECT_EQ(video_capture_settings.Height(),
            MediaStreamVideoSource::kDefaultHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(),
            MediaStreamVideoSource::kDefaultFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::RESOLUTION_POLICY_FIXED_RESOLUTION);
  EXPECT_EQ(video_capture_settings.PowerLineFrequency(),
            media::PowerLineFrequency::FREQUENCY_DEFAULT);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());

  const VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.max_width, MediaStreamVideoSource::kDefaultWidth);
  EXPECT_EQ(track_settings.max_height, MediaStreamVideoSource::kDefaultHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio,
            1.0 / MediaStreamVideoSource::kDefaultHeight);
  EXPECT_EQ(track_settings.max_aspect_ratio,
            MediaStreamVideoSource::kDefaultWidth);
  // 0.0 is the default max_frame_rate and it indicates no frame-rate adjustment
  EXPECT_EQ(track_settings.max_frame_rate, 0.0);
}

TEST_F(UserMediaClientImplTest, DefaultTabCapturePropagate) {
  MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(
      blink::WebString::FromASCII(kMediaStreamSourceTab));
  blink::WebMediaConstraints audio_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebMediaConstraints video_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(audio_constraints,
                                                   video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_client_impl_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_client_impl_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(std::string(), audio_capture_settings.device_id());
  EXPECT_FALSE(audio_capture_settings.hotword_enabled());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_FALSE(properties.enable_sw_echo_cancellation);
  EXPECT_FALSE(properties.disable_hw_echo_cancellation);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_FALSE(properties.goog_typing_noise_detection);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_beamforming);
  EXPECT_FALSE(properties.goog_highpass_filter);
  EXPECT_FALSE(properties.goog_experimental_auto_gain_control);
  EXPECT_TRUE(properties.goog_array_geometry.empty());

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(), kDefaultScreenCastWidth);
  EXPECT_EQ(video_capture_settings.Height(), kDefaultScreenCastHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(), kDefaultScreenCastFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::RESOLUTION_POLICY_FIXED_RESOLUTION);
  EXPECT_EQ(video_capture_settings.PowerLineFrequency(),
            media::PowerLineFrequency::FREQUENCY_DEFAULT);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());
  EXPECT_FALSE(video_capture_settings.max_frame_rate().has_value());

  const VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.max_width, kDefaultScreenCastWidth);
  EXPECT_EQ(track_settings.max_height, kDefaultScreenCastHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio, 1.0 / kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_aspect_ratio, kMaxScreenCastDimension);
  // 0.0 is the default max_frame_rate and it indicates no frame-rate adjustment
  EXPECT_EQ(track_settings.max_frame_rate, 0.0);
}

TEST_F(UserMediaClientImplTest, DefaultDesktopCapturePropagate) {
  MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(
      blink::WebString::FromASCII(kMediaStreamSourceDesktop));
  blink::WebMediaConstraints audio_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebMediaConstraints video_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(audio_constraints,
                                                   video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_client_impl_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_client_impl_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(std::string(), audio_capture_settings.device_id());
  EXPECT_FALSE(audio_capture_settings.hotword_enabled());
  EXPECT_FALSE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_FALSE(properties.enable_sw_echo_cancellation);
  EXPECT_FALSE(properties.disable_hw_echo_cancellation);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_FALSE(properties.goog_typing_noise_detection);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_beamforming);
  EXPECT_FALSE(properties.goog_highpass_filter);
  EXPECT_FALSE(properties.goog_experimental_auto_gain_control);
  EXPECT_TRUE(properties.goog_array_geometry.empty());

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(), kDefaultScreenCastWidth);
  EXPECT_EQ(video_capture_settings.Height(), kDefaultScreenCastHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(), kDefaultScreenCastFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT);
  EXPECT_EQ(video_capture_settings.PowerLineFrequency(),
            media::PowerLineFrequency::FREQUENCY_DEFAULT);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());
  EXPECT_FALSE(video_capture_settings.max_frame_rate().has_value());

  const VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.max_width, kDefaultScreenCastWidth);
  EXPECT_EQ(track_settings.max_height, kDefaultScreenCastHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio, 1.0 / kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_aspect_ratio, kMaxScreenCastDimension);
  // 0.0 is the default max_frame_rate and it indicates no frame-rate adjustment
  EXPECT_EQ(track_settings.max_frame_rate, 0.0);
}

TEST_F(UserMediaClientImplTest, NonDefaultAudioConstraintsPropagate) {
  MockConstraintFactory factory;
  factory.basic().device_id.SetExact(
      blink::WebString::FromASCII(kFakeAudioInputDeviceId1));
  factory.basic().hotword_enabled.SetExact(true);
  factory.basic().disable_local_echo.SetExact(true);
  factory.basic().render_to_associated_sink.SetExact(true);
  factory.basic().echo_cancellation.SetExact(false);
  factory.basic().goog_audio_mirroring.SetExact(true);
  factory.basic().goog_typing_noise_detection.SetExact(true);
  factory.basic().goog_array_geometry.SetExact(
      blink::WebString::FromASCII("1 1 1"));
  blink::WebMediaConstraints audio_constraints =
      factory.CreateWebMediaConstraints();
  // Request contains only audio
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(
          audio_constraints, blink::WebMediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_client_impl_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_client_impl_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  EXPECT_FALSE(video_capture_settings.HasValue());

  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(kFakeAudioInputDeviceId1, audio_capture_settings.device_id());
  EXPECT_TRUE(audio_capture_settings.hotword_enabled());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_TRUE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_FALSE(properties.enable_sw_echo_cancellation);
  EXPECT_TRUE(properties.disable_hw_echo_cancellation);
  EXPECT_TRUE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_TRUE(properties.goog_typing_noise_detection);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_beamforming);
  EXPECT_FALSE(properties.goog_highpass_filter);
  EXPECT_FALSE(properties.goog_experimental_auto_gain_control);
  const std::vector<media::Point> kGeometry = {{1.0, 1.0, 1.0}};
  EXPECT_EQ(kGeometry, properties.goog_array_geometry);
}

TEST_F(UserMediaClientImplTest, ObserveMediaDeviceChanges) {
  EXPECT_CALL(media_devices_dispatcher_, SubscribeDeviceChangeNotifications(
                                             MEDIA_DEVICE_TYPE_AUDIO_INPUT, _));
  EXPECT_CALL(media_devices_dispatcher_, SubscribeDeviceChangeNotifications(
                                             MEDIA_DEVICE_TYPE_VIDEO_INPUT, _));
  EXPECT_CALL(
      media_devices_dispatcher_,
      SubscribeDeviceChangeNotifications(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, _));
  user_media_client_impl_->SetMediaDeviceChangeObserver(
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

  user_media_client_impl_->SetMediaDeviceChangeObserver(
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
  mixed_desc.VideoTracks(video_tracks);
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_audio_device_counter());

  // Now we load a new document in the web frame. If in the above Stop() call,
  // UserMediaClientImpl accidentally removed audio track, then video track will
  // be removed again here, which is incorrect.
  LoadNewDocumentInFrame();
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, ms_dispatcher_->stop_video_device_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_audio_device_counter());
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryInvalidAudioDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kInvalidDeviceId);
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(
          audio_constraints, blink::WebMediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(UserMediaClientImplUnderTest::REQUEST_FAILED,
            user_media_client_impl_->request_state());
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryInvalidVideoDeviceId) {
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(kInvalidDeviceId);
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(blink::WebMediaConstraints(),
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
  // default audio or video device ID.
  std::string expected_audio_device_id =
      media::AudioDeviceDescription::kDefaultDeviceId;
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  expected_audio_device_id,
                                  kFakeVideoInputDeviceId1);
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
