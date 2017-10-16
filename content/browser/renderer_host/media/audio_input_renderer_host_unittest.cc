// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_renderer_host.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::media::AudioInputController;
using ::testing::_;
using ::testing::Assign;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace content {

namespace {

const int kStreamId = 100;
const int kRenderProcessId = 42;
const int kRenderFrameId = 31415;
const int kSharedMemoryCount = 11;
const double kNewVolume = 0.618;
const char kSecurityOrigin[] = "http://localhost";

url::Origin SecurityOrigin() {
  return url::Origin(GURL(kSecurityOrigin));
}

AudioInputHostMsg_CreateStream_Config DefaultConfig() {
  AudioInputHostMsg_CreateStream_Config config;
  config.params = media::AudioParameters::UnavailableDeviceParams();
  config.automatic_gain_control = false;
  config.shared_memory_count = kSharedMemoryCount;
  return config;
}

class MockRenderer {
 public:
  MOCK_METHOD4(NotifyStreamCreated,
               void(int /*stream_id*/,
                    base::SharedMemoryHandle /*handle*/,
                    base::SyncSocket::TransitDescriptor /*socket_desriptor*/,
                    bool initially_muted));
  MOCK_METHOD1(NotifyStreamError, void(int /*stream_id*/));
  MOCK_METHOD0(WasShutDown, void());
};

// This class overrides Send to intercept the messages that the
// AudioInputRendererHost sends to the renderer. They are sent to
// the provided MockRenderer instead.
class AudioInputRendererHostWithInterception : public AudioInputRendererHost {
 public:
  AudioInputRendererHostWithInterception(
      int render_process_id,
      media::AudioManager* audio_manager,
      MediaStreamManager* media_stream_manager,
      AudioMirroringManager* audio_mirroring_manager,
      media::UserInputMonitor* user_input_monitor,
      MockRenderer* renderer)
      : AudioInputRendererHost(render_process_id,
                               audio_manager,
                               media_stream_manager,
                               audio_mirroring_manager,
                               user_input_monitor),
        renderer_(renderer) {
    set_peer_process_for_testing(base::Process::Current());
  }

 protected:
  ~AudioInputRendererHostWithInterception() override = default;

 private:
  bool Send(IPC::Message* message) override {
    bool handled = true;

    IPC_BEGIN_MESSAGE_MAP(AudioInputRendererHostWithInterception, *message)
      IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamCreated,
                          NotifyRendererStreamCreated)
      IPC_MESSAGE_HANDLER(AudioInputMsg_NotifyStreamError,
                          NotifyRendererStreamError)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    EXPECT_TRUE(handled);
    delete message;
    return true;
  }

  void ShutdownForBadMessage() override { renderer_->WasShutDown(); }

  void NotifyRendererStreamCreated(
      int stream_id,
      base::SharedMemoryHandle handle,
      base::SyncSocket::TransitDescriptor socket_descriptor,
      bool initially_muted) {
    // It's difficult to check that the sync socket and shared memory is
    // valid in the gmock macros, so we check them here.
    EXPECT_NE(base::SyncSocket::UnwrapHandle(socket_descriptor),
              base::SyncSocket::kInvalidHandle);
    size_t length = handle.GetSize();
    base::SharedMemory memory(handle, /*read_only*/ true);
    EXPECT_TRUE(memory.Map(length));
    renderer_->NotifyStreamCreated(stream_id, handle, socket_descriptor,
                                   initially_muted);
    EXPECT_TRUE(memory.Unmap());
    memory.Close();
  }

  void NotifyRendererStreamError(int stream_id) {
    renderer_->NotifyStreamError(stream_id);
  }

  MockRenderer* renderer_;
};

class MockAudioInputController : public AudioInputController {
 public:
  MockAudioInputController(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      AudioInputController::SyncWriter* writer,
      media::AudioManager* audio_manager,
      AudioInputController::EventHandler* event_handler,
      media::UserInputMonitor* user_input_monitor,
      const media::AudioParameters& params,
      StreamType type)
      : AudioInputController(std::move(task_runner),
                             event_handler,
                             writer,
                             user_input_monitor,
                             params,
                             type) {
    GetTaskRunnerForTesting()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioInputController::EventHandler::OnCreated,
                       base::Unretained(event_handler), base::Unretained(this),
                       false));
  }

  EventHandler* handler() { return GetHandlerForTesting(); }

  void Close(base::OnceClosure cl) override {
    DidClose();
    // Hop to audio manager thread before calling task, since this is the real
    // behavior.
    GetTaskRunnerForTesting()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([]() {}), std::move(cl));
  }

  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));

  MOCK_METHOD0(DidClose, void());

  // AudioInputCallback impl, irrelevant to us.
  MOCK_METHOD4(
      OnData,
      void(media::AudioInputStream*, const media::AudioBus*, uint32_t, double));
  MOCK_METHOD1(OnError, void(media::AudioInputStream*));

 protected:
  ~MockAudioInputController() override = default;
};

class MockControllerFactory : public AudioInputController::Factory {
 public:
  using MockController = StrictMock<MockAudioInputController>;

  MockControllerFactory() {
    AudioInputController::set_factory_for_testing(this);
  }

  ~MockControllerFactory() override {
    AudioInputController::set_factory_for_testing(nullptr);
  }

  // AudioInputController::Factory implementation:
  AudioInputController* Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      AudioInputController::SyncWriter* sync_writer,
      media::AudioManager* audio_manager,
      AudioInputController::EventHandler* event_handler,
      media::AudioParameters params,
      media::UserInputMonitor* user_input_monitor,
      AudioInputController::StreamType type) override {
    ControllerCreated();
    scoped_refptr<MockController> controller =
        new MockController(task_runner, sync_writer, audio_manager,
                           event_handler, user_input_monitor, params, type);
    controller_list_.push_back(controller);
    return controller.get();
  }

  MockController* controller(size_t i) {
    EXPECT_GT(controller_list_.size(), i);
    return controller_list_[i].get();
  }

  MOCK_METHOD0(ControllerCreated, void());

 private:
  std::vector<scoped_refptr<MockController>> controller_list_;
};

}  // namespace

class AudioInputRendererHostTest : public testing::Test {
 public:
  AudioInputRendererHostTest() {
    base::CommandLine* flags = base::CommandLine::ForCurrentProcess();
    flags->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    flags->AppendSwitch(switches::kUseFakeUIForMediaStream);

    audio_manager_ = std::make_unique<media::FakeAudioManager>(
        std::make_unique<media::TestAudioThread>(), &log_factory_);
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), audio_manager_->GetTaskRunner());
    airh_ = new AudioInputRendererHostWithInterception(
        kRenderProcessId, media::AudioManager::Get(),
        media_stream_manager_.get(), AudioMirroringManager::GetInstance(),
        nullptr, &renderer_);
  }

  ~AudioInputRendererHostTest() override {
    airh_->OnChannelClosing();
    base::RunLoop().RunUntilIdle();

    audio_manager_->Shutdown();
  }

 protected:
  // Makes device |device_id| with name |name| available to open with the
  // session id returned.
  int Open(const std::string& device_id, const std::string& name) {
    int session_id = media_stream_manager_->audio_input_device_manager()->Open(
        MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, device_id, name));
    base::RunLoop().RunUntilIdle();
    return session_id;
  }

  std::string GetRawNondefaultId() {
    std::string id;
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;

    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(
            [](std::string* id, const MediaDeviceEnumeration& result) {
              // Index 0 is default, so use 1.
              CHECK(result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT]
                        .size() > 1)
                  << "Expected to have a nondefault device.";
              *id = result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT][1]
                        .device_id;
            },
            base::Unretained(&id)));
    base::RunLoop().RunUntilIdle();
    return id;
  }

  media::FakeAudioLogFactory log_factory_;
  StrictMock<MockControllerFactory> controller_factory_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  StrictMock<MockRenderer> renderer_;
  scoped_refptr<AudioInputRendererHost> airh_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputRendererHostTest);
};

// Checks that a controller is created and a reply is sent when creating a
// stream.
TEST_F(AudioInputRendererHostTest, CreateWithDefaultDevice) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));

  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
}

// If authorization hasn't been granted, only reply with and error and do
// nothing else.
TEST_F(AudioInputRendererHostTest, CreateWithoutAuthorization_Error) {
  EXPECT_CALL(renderer_, NotifyStreamError(kStreamId));

  int session_id = 0;
  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();
}

// Like CreateWithDefaultDevice but with a nondefault device.
TEST_F(AudioInputRendererHostTest, CreateWithNonDefaultDevice) {
  int session_id = Open("Nondefault device", GetRawNondefaultId());

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));

  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
}

// Checks that stream is started when calling record.
TEST_F(AudioInputRendererHostTest, CreateRecordClose) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), Record());
  EXPECT_CALL(*controller_factory_.controller(0), DidClose());

  airh_->OnMessageReceived(AudioInputHostMsg_RecordStream(kStreamId));
  base::RunLoop().RunUntilIdle();
  airh_->OnMessageReceived(AudioInputHostMsg_CloseStream(kStreamId));
  base::RunLoop().RunUntilIdle();
}

// In addition to the above, also check that a SetVolume message is propagated
// to the controller.
TEST_F(AudioInputRendererHostTest, CreateSetVolumeRecordClose) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), SetVolume(kNewVolume));
  EXPECT_CALL(*controller_factory_.controller(0), Record());
  EXPECT_CALL(*controller_factory_.controller(0), DidClose());

  airh_->OnMessageReceived(AudioInputHostMsg_SetVolume(kStreamId, kNewVolume));
  airh_->OnMessageReceived(AudioInputHostMsg_RecordStream(kStreamId));
  airh_->OnMessageReceived(AudioInputHostMsg_CloseStream(kStreamId));

  base::RunLoop().RunUntilIdle();
}

// Check that a too large volume is treated like a bad message and doesn't
// reach the controller.
TEST_F(AudioInputRendererHostTest, SetVolumeTooLarge_BadMessage) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
  EXPECT_CALL(renderer_, WasShutDown());

  airh_->OnMessageReceived(AudioInputHostMsg_SetVolume(kStreamId, 5));

  base::RunLoop().RunUntilIdle();
}

// Like above.
TEST_F(AudioInputRendererHostTest, SetVolumeNegative_BadMessage) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
  EXPECT_CALL(renderer_, WasShutDown());

  airh_->OnMessageReceived(AudioInputHostMsg_SetVolume(kStreamId, -0.5));

  base::RunLoop().RunUntilIdle();
}

// Checks that a stream_id cannot be reused.
TEST_F(AudioInputRendererHostTest, CreateTwice_Error) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(renderer_, NotifyStreamError(kStreamId));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
}

// Checks that when two streams are created, messages are routed to the correct
// stream.
TEST_F(AudioInputRendererHostTest, TwoStreams) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId + 1, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated()).Times(2);

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId + 1, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), Record());
  airh_->OnMessageReceived(AudioInputHostMsg_RecordStream(kStreamId));
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*controller_factory_.controller(1), SetVolume(kNewVolume));
  airh_->OnMessageReceived(
      AudioInputHostMsg_SetVolume(kStreamId + 1, kNewVolume));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
  EXPECT_CALL(*controller_factory_.controller(1), DidClose());
}

// Checks that the stream is properly cleaned up and a notification is sent to
// the renderer when the stream encounters an error.
TEST_F(AudioInputRendererHostTest, Error_ClosesController) {
  int session_id =
      Open("Default device", media::AudioDeviceDescription::kDefaultDeviceId);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));

  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
  EXPECT_CALL(renderer_, NotifyStreamError(kStreamId));

  controller_factory_.controller(0)->handler()->OnError(
      controller_factory_.controller(0), AudioInputController::UNKNOWN_ERROR);

  // Check Close expectation before the destructor.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(controller_factory_.controller(0));
}

// Checks that tab capture streams can be created.
TEST_F(AudioInputRendererHostTest, TabCaptureStream) {
  StreamControls controls(/* request_audio */ true, /* request_video */ false);
  controls.audio.device_id = base::StringPrintf(
      "web-contents-media-stream://%d:%d", kRenderProcessId, kRenderFrameId);
  controls.audio.stream_source = kMediaStreamSourceTab;
  std::string request_label = media_stream_manager_->MakeMediaAccessRequest(
      kRenderProcessId, kRenderFrameId, 0, controls, SecurityOrigin(),
      base::BindOnce([](const MediaStreamDevices& devices,
                        std::unique_ptr<MediaStreamUIProxy>) {}));
  base::RunLoop().RunUntilIdle();
  int session_id = Open("Tab capture", controls.audio.device_id);

  EXPECT_CALL(renderer_, NotifyStreamCreated(kStreamId, _, _, _));
  EXPECT_CALL(controller_factory_, ControllerCreated());

  airh_->OnMessageReceived(AudioInputHostMsg_CreateStream(
      kStreamId, kRenderFrameId, session_id, DefaultConfig()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*controller_factory_.controller(0), DidClose());
}

}  // namespace content
