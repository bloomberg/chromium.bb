// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace content {

namespace {

constexpr int kProcessId = 5;
constexpr int kRenderId = 6;
constexpr int kPageRequestId = 7;
constexpr const char* kRegularVideoDeviceId = "stub_device_0";
constexpr const char* kDepthVideoDeviceId = "stub_device_1 (depth)";
constexpr media::VideoCaptureApi kStubCaptureApi =
    media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
constexpr double kStubFocalLengthX = 135.0;
constexpr double kStubFocalLengthY = 135.6;
constexpr double kStubDepthNear = 0.0;
constexpr double kStubDepthFar = 65.535;

void AudioInputDevicesEnumerated(base::Closure quit_closure,
                                 media::AudioDeviceDescriptions* out,
                                 const MediaDeviceEnumeration& enumeration) {
  for (const auto& info : enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT]) {
    out->emplace_back(info.label, info.device_id, info.group_id);
  }
  quit_closure.Run();
}

}  // anonymous namespace

class MockMediaStreamDispatcherHost : public MediaStreamDispatcherHost,
                                      public mojom::MediaStreamDeviceObserver {
 public:
  MockMediaStreamDispatcherHost(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      MediaStreamManager* manager)
      : MediaStreamDispatcherHost(kProcessId, manager),
        task_runner_(task_runner) {}
  ~MockMediaStreamDispatcherHost() override {}

  // A list of mock methods.
  MOCK_METHOD3(OnStreamGenerationSuccess,
               void(int request_id,
                    int audio_array_size,
                    int video_array_size));
  MOCK_METHOD2(OnStreamGenerationFailure,
               void(int request_id, MediaStreamRequestResult result));
  MOCK_METHOD0(OnDeviceStopSuccess, void());
  MOCK_METHOD0(OnDeviceOpenSuccess, void());

  // Accessor to private functions.
  void OnGenerateStream(int render_frame_id,
                        int page_request_id,
                        const StreamControls& controls,
                        const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::GenerateStream(
        render_frame_id, page_request_id, controls, false,
        base::BindOnce(&MockMediaStreamDispatcherHost::OnStreamGenerated,
                       base::Unretained(this), page_request_id));
  }

  void OnStopStreamDevice(int render_frame_id,
                          const std::string& device_id) {
    MediaStreamDispatcherHost::StopStreamDevice(render_frame_id, device_id);
  }

  void OnOpenDevice(int render_frame_id,
                    int page_request_id,
                    const std::string& device_id,
                    MediaStreamType type,
                    const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OpenDevice(
        render_frame_id, page_request_id, device_id, type,
        base::BindOnce(&MockMediaStreamDispatcherHost::OnDeviceOpened,
                       base::Unretained(this)));
  }

  void OnStreamStarted(const std::string& label) {
    MediaStreamDispatcherHost::OnStreamStarted(label);
  }

  // mojom::MediaStreamDeviceObserver implementation.
  void OnDeviceStopped(const std::string& label,
                       const MediaStreamDevice& device) override {
    OnDeviceStoppedInternal(label, device);
  }

  mojom::MediaStreamDeviceObserverPtr CreateInterfacePtrAndBind() {
    mojom::MediaStreamDeviceObserverPtr observer;
    bindings_.AddBinding(this, mojo::MakeRequest(&observer));
    return observer;
  }

  std::string label_;
  MediaStreamDevices audio_devices_;
  MediaStreamDevices video_devices_;
  MediaStreamDevice opened_device_;

 private:
  // These handler methods do minimal things and delegate to the mock methods.
  void OnStreamGenerated(int request_id,
                         MediaStreamRequestResult result,
                         const std::string& label,
                         const MediaStreamDevices& audio_devices,
                         const MediaStreamDevices& video_devices) {
    if (result != MEDIA_DEVICE_OK) {
      OnStreamGenerationFailed(request_id, result);
      return;
    }

    OnStreamGenerationSuccess(request_id, audio_devices.size(),
                              video_devices.size());
    // Simulate the stream started event back to host for UI testing.
    OnStreamStarted(label);

    // Notify that the event have occurred.
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));

    label_ = label;
    audio_devices_ = audio_devices;
    video_devices_ = video_devices;
  }

  void OnStreamGenerationFailed(int request_id,
                                MediaStreamRequestResult result) {
    OnStreamGenerationFailure(request_id, result);
    if (!quit_closures_.empty()) {
      base::Closure quit_closure = quit_closures_.front();
      quit_closures_.pop();
      task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    }

    label_.clear();
  }

  void OnDeviceStoppedInternal(const std::string& label,
                               const MediaStreamDevice& device) {
    if (IsVideoMediaType(device.type))
      EXPECT_TRUE(device.IsSameDevice(video_devices_[0]));
    if (IsAudioInputMediaType(device.type))
      EXPECT_TRUE(device.IsSameDevice(audio_devices_[0]));

    OnDeviceStopSuccess();
  }

  void OnDeviceOpened(bool success,
                      const std::string& label,
                      const MediaStreamDevice& device) {
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    if (success) {
      label_ = label;
      opened_device_ = device;
      OnDeviceOpenSuccess();
    }
  }

  mojo::BindingSet<mojom::MediaStreamDeviceObserver> bindings_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::queue<base::Closure> quit_closures_;
};

class MockMediaStreamUIProxy : public FakeMediaStreamUIProxy {
 public:
  MockMediaStreamUIProxy()
      : FakeMediaStreamUIProxy(/*tests_use_fake_render_frame_hosts=*/true) {}
  void OnStarted(
      base::OnceClosure stop,
      MediaStreamUIProxy::WindowIdCallback window_id_callback) override {
    // gmock cannot handle move-only types:
    MockOnStarted(base::AdaptCallbackForRepeating(std::move(stop)));
  }

  MOCK_METHOD1(MockOnStarted, void(base::Closure stop));
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  MediaStreamDispatcherHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        origin_(url::Origin::Create(GURL("https://test.com"))) {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    browser_context_ = std::make_unique<TestBrowserContext>();
    // Make sure we use fake devices to avoid long delays.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    auto mock_video_capture_provider =
        std::make_unique<MockVideoCaptureProvider>();
    mock_video_capture_provider_ = mock_video_capture_provider.get();
    // Create our own MediaStreamManager.
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), audio_manager_->GetTaskRunner(),
        std::move(mock_video_capture_provider));

    host_ = std::make_unique<MockMediaStreamDispatcherHost>(
        base::ThreadTaskRunnerHandle::Get(), media_stream_manager_.get());
    host_->set_salt_and_origin_callback_for_testing(
        base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                            base::Unretained(this)));
    mojom::MediaStreamDeviceObserverPtr observer =
        host_->CreateInterfacePtrAndBind();
    host_->SetMediaStreamDeviceObserverForTesting(kRenderId,
                                                  std::move(observer));

#if defined(OS_CHROMEOS)
    chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  }

  ~MediaStreamDispatcherHostTest() override {
    audio_manager_->Shutdown();
#if defined(OS_CHROMEOS)
    chromeos::CrasAudioHandler::Shutdown();
#endif
  }

  void SetUp() override {
    stub_video_device_ids_.emplace_back(kRegularVideoDeviceId);
    stub_video_device_ids_.emplace_back(kDepthVideoDeviceId);
    ON_CALL(*mock_video_capture_provider_, DoGetDeviceInfosAsync(_))
        .WillByDefault(Invoke(
            [this](
                VideoCaptureProvider::GetDeviceInfosCallback& result_callback) {
              std::vector<media::VideoCaptureDeviceInfo> result;
              for (const auto& device_id : stub_video_device_ids_) {
                media::VideoCaptureDeviceInfo info;
                info.descriptor.device_id = device_id;
                info.descriptor.capture_api = kStubCaptureApi;
                if (device_id == kDepthVideoDeviceId) {
                  info.descriptor.camera_calibration.emplace();
                  info.descriptor.camera_calibration->focal_length_x =
                      kStubFocalLengthX;
                  info.descriptor.camera_calibration->focal_length_y =
                      kStubFocalLengthY;
                  info.descriptor.camera_calibration->depth_near =
                      kStubDepthNear;
                  info.descriptor.camera_calibration->depth_far = kStubDepthFar;
                }
                result.push_back(info);
              }
              base::ResetAndReturn(&result_callback).Run(result);
            }));

    base::RunLoop run_loop;
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(&AudioInputDevicesEnumerated, run_loop.QuitClosure(),
                   &audio_device_descriptions_));
    run_loop.Run();

    ASSERT_GT(audio_device_descriptions_.size(), 0u);
  }

  void TearDown() override { host_.reset(); }

 protected:
  std::unique_ptr<FakeMediaStreamUIProxy> CreateMockUI(bool expect_started) {
    std::unique_ptr<MockMediaStreamUIProxy> fake_ui =
        std::make_unique<MockMediaStreamUIProxy>();
    if (expect_started)
      EXPECT_CALL(*fake_ui, MockOnStarted(_));
    return fake_ui;
  }

  virtual void SetupFakeUI(bool expect_started) {
    media_stream_manager_->UseFakeUIFactoryForTests(
        base::Bind(&MediaStreamDispatcherHostTest::CreateMockUI,
                   base::Unretained(this), expect_started));
  }

  void GenerateStreamAndWaitForResult(int render_frame_id,
                                      int page_request_id,
                                      const StreamControls& controls) {
    base::RunLoop run_loop;
    int expected_audio_array_size =
        (controls.audio.requested && !audio_device_descriptions_.empty()) ? 1
                                                                          : 0;
    int expected_video_array_size =
        (controls.video.requested && !stub_video_device_ids_.empty()) ? 1 : 0;
    EXPECT_CALL(*host_, OnStreamGenerationSuccess(page_request_id,
                                                  expected_audio_array_size,
                                                  expected_video_array_size));
    host_->OnGenerateStream(render_frame_id, page_request_id, controls,
                            run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->audio_devices_));
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->audio_devices_, origin_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void GenerateStreamAndWaitForFailure(
      int render_frame_id,
      int page_request_id,
      const StreamControls& controls,
      MediaStreamRequestResult expected_result) {
      base::RunLoop run_loop;
      EXPECT_CALL(*host_,
                  OnStreamGenerationFailure(page_request_id, expected_result));
      host_->OnGenerateStream(render_frame_id, page_request_id, controls,
                              run_loop.QuitClosure());
      run_loop.Run();
  }

  void OpenVideoDeviceAndWaitForResult(int render_frame_id,
                                       int page_request_id,
                                       const std::string& device_id) {
    EXPECT_CALL(*host_, OnDeviceOpenSuccess());
    base::RunLoop run_loop;
    host_->OnOpenDevice(render_frame_id, page_request_id, device_id,
                        MEDIA_DEVICE_VIDEO_CAPTURE, run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void OpenVideoDeviceAndWaitForFailure(int render_frame_id,
                                        int page_request_id,
                                        const std::string& device_id) {
    EXPECT_CALL(*host_, OnDeviceOpenSuccess()).Times(0);
    base::RunLoop run_loop;
    host_->OnOpenDevice(render_frame_id, page_request_id, device_id,
                        MEDIA_DEVICE_VIDEO_CAPTURE, run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_FALSE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  bool DoesContainRawIds(const MediaStreamDevices& devices) {
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].id != media::AudioDeviceDescription::kDefaultDeviceId &&
          devices[i].id !=
              media::AudioDeviceDescription::kCommunicationsDeviceId) {
        for (const auto& audio_device : audio_device_descriptions_) {
          if (audio_device.unique_id == devices[i].id)
            return true;
        }
      }
      for (const std::string& device_id : stub_video_device_ids_) {
        if (device_id == devices[i].id)
          return true;
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(const MediaStreamDevices& devices,
                                 const url::Origin& origin) {
    for (size_t i = 0; i < devices.size(); ++i) {
      bool found_match = false;
      media::AudioDeviceDescriptions::const_iterator audio_it =
          audio_device_descriptions_.begin();
      for (; audio_it != audio_device_descriptions_.end(); ++audio_it) {
        if (DoesMediaDeviceIDMatchHMAC(browser_context_->GetMediaDeviceIDSalt(),
                                       origin, devices[i].id,
                                       audio_it->unique_id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      for (const std::string& device_id : stub_video_device_ids_) {
        if (DoesMediaDeviceIDMatchHMAC(browser_context_->GetMediaDeviceIDSalt(),
                                       origin, devices[i].id, device_id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      if (!found_match)
        return false;
    }
    return true;
  }

  std::pair<std::string, url::Origin> GetSaltAndOrigin(int /* process_id */,
                                                       int /* frame_id */) {
    return std::make_pair(browser_context_->GetMediaDeviceIDSalt(), origin_);
  }

  std::unique_ptr<MockMediaStreamDispatcherHost> host_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  media::AudioDeviceDescriptions audio_device_descriptions_;
  std::vector<std::string> stub_video_device_ids_;
  url::Origin origin_;
  MockVideoCaptureProvider* mock_video_capture_provider_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithVideoOnly) {
  StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioOnly) {
  StreamControls controls(true, false);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 0u);
}

// This test simulates a shutdown scenario: we don't setup a fake UI proxy for
// MediaStreamManager, so it will create an ordinary one which will not find
// a RenderFrameHostDelegate. This normally should only be the case at shutdown.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithNothing) {
  StreamControls controls(false, false);

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioAndVideo) {
  StreamControls controls(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithDepthVideo) {
  // We specify to generate both audio and video stream.
  StreamControls controls(true, true);
  std::string source_id = GetHMACForMediaDeviceID(
      browser_context_->GetMediaDeviceIDSalt(), origin_, kDepthVideoDeviceId);
  // |source_id| corresponds to the depth device. As we can generate only one
  // video stream using GenerateStreamAndWaitForResult, we use
  // controls.video.source_id to specify that the stream is depth video.
  // See also MediaStreamManager::GenerateStream and other tests here.
  controls.video.device_id = source_id;

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  // We specified the generation and expect to get
  // one audio and one depth video stream.
  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  // host_->video_devices_[0] contains the information about generated video
  // stream device (the depth device).
  const base::Optional<CameraCalibration> calibration =
      host_->video_devices_[0].camera_calibration;
  EXPECT_TRUE(calibration);
  EXPECT_EQ(calibration->focal_length_x, kStubFocalLengthX);
  EXPECT_EQ(calibration->focal_length_y, kStubFocalLengthY);
  EXPECT_EQ(calibration->depth_near, kStubDepthNear);
  EXPECT_EQ(calibration->depth_far, kStubDepthFar);
}

// This test generates two streams with video only using the same render frame
// id. The same capture device with the same device and session id is expected
// to be used.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsFromSameRenderId) {
  StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream.
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId + 1, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().id;
  int session_id2 = host_->video_devices_.front().session_id;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_EQ(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamAndOpenDeviceFromSameRenderId) {
  SetupFakeUI(true);
  StreamControls controls(false, true);

  // Generate first stream.
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kRenderId, kPageRequestId, device_id1);

  const std::string device_id2 = host_->opened_device_.id;
  const int session_id2 = host_->opened_device_.session_id;
  const std::string label2 = host_->label_;

  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

// This test generates two streams with video only using two separate render
// frame ids. The same device id but different session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsDifferentRenderId) {
  StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream from another render frame.
  mojom::MediaStreamDeviceObserverPtr observer =
      host_->CreateInterfacePtrAndBind();
  host_->SetMediaStreamDeviceObserverForTesting(kRenderId + 1,
                                                std::move(observer));
  GenerateStreamAndWaitForResult(kRenderId + 1, kPageRequestId + 1, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().id;
  const int session_id2 = host_->video_devices_.front().session_id;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

// This test request two streams with video only without waiting for the first
// stream to be generated before requesting the second.
// The same device id and session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithoutWaiting) {
  StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  {
    InSequence s;
    EXPECT_CALL(*host_, OnStreamGenerationSuccess(kPageRequestId, 0, 1));

    // Generate second stream.
    EXPECT_CALL(*host_, OnStreamGenerationSuccess(kPageRequestId + 1, 0, 1));
  }
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  host_->OnGenerateStream(kRenderId, kPageRequestId, controls,
                          run_loop1.QuitClosure());
  host_->OnGenerateStream(kRenderId, kPageRequestId + 1, controls,
                          run_loop2.QuitClosure());

  run_loop1.Run();
  run_loop2.Run();
}

// Test that we can generate streams where a sourceId is specified in
// the request.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithSourceId) {
  ASSERT_GE(audio_device_descriptions_.size(), 1u);
  ASSERT_GE(stub_video_device_ids_.size(), 1u);

  media::AudioDeviceDescriptions::const_iterator audio_it =
      audio_device_descriptions_.begin();
  for (; audio_it != audio_device_descriptions_.end(); ++audio_it) {
    std::string source_id = GetHMACForMediaDeviceID(
        browser_context_->GetMediaDeviceIDSalt(), origin_, audio_it->unique_id);
    ASSERT_FALSE(source_id.empty());
    StreamControls controls(true, true);
    controls.audio.device_id = source_id;

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
    EXPECT_EQ(host_->audio_devices_[0].id, source_id);
  }

  for (const std::string& device_id : stub_video_device_ids_) {
    std::string source_id = GetHMACForMediaDeviceID(
        browser_context_->GetMediaDeviceIDSalt(), origin_, device_id);
    ASSERT_FALSE(source_id.empty());
    StreamControls controls(true, true);
    controls.video.device_id = source_id;

    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
    EXPECT_EQ(host_->video_devices_[0].id, source_id);
  }
}

// Test that generating a stream with an invalid video source id fail.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithInvalidVideoSourceId) {
  StreamControls controls(true, true);
  controls.video.device_id = "invalid source id";

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_NO_HARDWARE);
}

// Test that generating a stream with an invalid audio source id fail.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithInvalidAudioSourceId) {
  StreamControls controls(true, true);
  controls.audio.device_id = "invalid source id";

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_NO_HARDWARE);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsNoAvailableVideoDevice) {
  stub_video_device_ids_.clear();
  StreamControls controls(true, true);

  SetupFakeUI(false);
  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_NO_HARDWARE);
}

// Test that if a OnStopStreamDevice message is received for a device that has
// been opened in a MediaStream and by pepper, the device is only stopped for
// the MediaStream.
TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStream) {
  StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  std::string stream_request_label = host_->label_;
  MediaStreamDevice video_device = host_->video_devices_.front();
  ASSERT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      stream_request_label).size());

  // Open the same device by Pepper.
  OpenVideoDeviceAndWaitForResult(kRenderId, kPageRequestId, video_device.id);
  std::string open_device_request_label = host_->label_;

  // Stop the device in the MediaStream.
  host_->OnStopStreamDevice(kRenderId, video_device.id);

  EXPECT_EQ(0u, media_stream_manager_->GetDevicesOpenedByRequest(
      stream_request_label).size());
  EXPECT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      open_device_request_label).size());
}

TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStreamAndRestart) {
  StreamControls controls(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  std::string request_label1 = host_->label_;
  MediaStreamDevice video_device = host_->video_devices_.front();
  // Expect that 1 audio and 1 video device has been opened.
  EXPECT_EQ(2u, media_stream_manager_->GetDevicesOpenedByRequest(
      request_label1).size());

  host_->OnStopStreamDevice(kRenderId, video_device.id);
  EXPECT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      request_label1).size());

  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
  std::string request_label2 = host_->label_;

  MediaStreamDevices request1_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1);
  MediaStreamDevices request2_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label2);

  ASSERT_EQ(1u, request1_devices.size());
  ASSERT_EQ(2u, request2_devices.size());

  // Test that the same audio device has been opened in both streams.
  EXPECT_TRUE(request1_devices[0].IsSameDevice(request2_devices[0]) ||
              request1_devices[0].IsSameDevice(request2_devices[1]));
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateTwoStreamsAndStopDeviceWhileWaitingForSecondStream) {
  StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  // Generate a second stream.
  EXPECT_CALL(*host_, OnStreamGenerationSuccess(kPageRequestId + 1, 0, 1));

  base::RunLoop run_loop1;
  host_->OnGenerateStream(kRenderId, kPageRequestId + 1, controls,
                          run_loop1.QuitClosure());

  // Stop the video stream device from stream 1 while waiting for the
  // second stream to be generated.
  host_->OnStopStreamDevice(kRenderId, host_->video_devices_[0].id);
  run_loop1.Run();

  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, CancelPendingStreams) {
  StreamControls controls(false, true);

  base::RunLoop run_loop;

  // Create multiple GenerateStream requests.
  size_t streams = 5;
  for (size_t i = 1; i <= streams; ++i) {
    host_->OnGenerateStream(kRenderId, kPageRequestId + i, controls,
                            run_loop.QuitClosure());
  }

  media_stream_manager_->CancelAllRequests(kProcessId);
  run_loop.RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, StopGeneratedStreams) {
  StreamControls controls(false, true);

  SetupFakeUI(true);

  // Create first group of streams.
  size_t generated_streams = 3;
  for (size_t i = 0; i < generated_streams; ++i)
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId + i, controls);

  media_stream_manager_->CancelAllRequests(kProcessId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, CloseFromUI) {
  StreamControls controls(false, true);

  base::Closure close_callback;
  media_stream_manager_->UseFakeUIFactoryForTests(base::Bind(
      [](base::Closure* close_callback) {
        std::unique_ptr<FakeMediaStreamUIProxy> stream_ui =
            std::make_unique<MockMediaStreamUIProxy>();
        EXPECT_CALL(*static_cast<MockMediaStreamUIProxy*>(stream_ui.get()),
                    MockOnStarted(_))
            .WillOnce(SaveArg<0>(close_callback));
        return stream_ui;
      },
      &close_callback));

  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  ASSERT_FALSE(close_callback.is_null());
  EXPECT_CALL(*host_, OnDeviceStopSuccess());
  close_callback.Run();
  base::RunLoop().RunUntilIdle();
}

// Test that the observer is notified if a video device that is in use is
// being unplugged.
TEST_F(MediaStreamDispatcherHostTest, VideoDeviceUnplugged) {
  StreamControls controls(true, true);
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  stub_video_device_ids_.clear();

  base::RunLoop run_loop;
  EXPECT_CALL(*host_, OnDeviceStopSuccess())
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  media_stream_manager_->media_devices_manager()->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  run_loop.Run();
}

// Test that changing the salt invalidates device IDs. Attempts to open an
// invalid device ID result in failure.
TEST_F(MediaStreamDispatcherHostTest, Salt) {
  SetupFakeUI(true);
  StreamControls controls(false, true);

  // Generate first stream.
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kRenderId, kPageRequestId, device_id1);
  const std::string device_id2 = host_->opened_device_.id;
  const int session_id2 = host_->opened_device_.session_id;
  const std::string label2 = host_->label_;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);

  // Reset salt and try to generate third stream with the invalidated device ID.
  browser_context_ = std::make_unique<TestBrowserContext>();
  EXPECT_CALL(*host_, OnDeviceOpenSuccess()).Times(0);
  OpenVideoDeviceAndWaitForFailure(kRenderId, kPageRequestId, device_id1);
  // Last open device ID and session are from the second stream.
  EXPECT_EQ(session_id2, host_->opened_device_.session_id);
  EXPECT_EQ(device_id2, host_->opened_device_.id);
}

};  // namespace content
