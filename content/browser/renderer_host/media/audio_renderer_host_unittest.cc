// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/audio_messages.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message_utils.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::NotNull;

namespace content {

namespace {
const int kRenderFrameId = 5;
const int kStreamId = 50;
const char kSecurityOrigin[] = "http://localhost";
const char kBadSecurityOrigin[] = "about:about";
const char kDefaultDeviceId[] = "";
const char kSalt[] = "salt";
const char kNondefaultDeviceId[] =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
const char kBadDeviceId[] =
    "badbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbad1";
const char kInvalidDeviceId[] = "invalid-device-id";

void ValidateRenderFrameId(int render_process_id,
                           int render_frame_id,
                           const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool frame_exists = (render_frame_id == kRenderFrameId);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, frame_exists));
}


class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MockAudioMirroringManager() {}
  virtual ~MockAudioMirroringManager() {}

  MOCK_METHOD3(AddDiverter,
               void(int render_process_id,
                    int render_frame_id,
                    Diverter* diverter));
  MOCK_METHOD1(RemoveDiverter, void(Diverter* diverter));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioMirroringManager);
};

class MockRenderProcessHostWithSignaling : public MockRenderProcessHost {
 public:
  MockRenderProcessHostWithSignaling(BrowserContext* context,
                                     base::RunLoop* auth_run_loop)
      : MockRenderProcessHost(context), auth_run_loop_(auth_run_loop) {}

  void ShutdownForBadMessage(CrashReportMode crash_report_mode) override {
    MockRenderProcessHost::ShutdownForBadMessage(crash_report_mode);
    auth_run_loop_->Quit();
  }

 private:
  base::RunLoop* auth_run_loop_;
};

class FakeAudioManagerWithAssociations : public media::FakeAudioManager {
 public:
  FakeAudioManagerWithAssociations(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      media::AudioLogFactory* factory)
      : FakeAudioManager(task_runner, task_runner, factory) {}

  void CreateDeviceAssociation(const std::string& input_device_id,
                               const std::string& output_device_id) {
    // We shouldn't accidentally add hashed ids, since the audio manager
    // works with raw ids.
    EXPECT_FALSE(IsValidDeviceId(input_device_id));
    EXPECT_FALSE(IsValidDeviceId(output_device_id));

    associations_[input_device_id] = output_device_id;
  }

  std::string GetAssociatedOutputDeviceID(
      const std::string& input_id) override {
    auto it = associations_.find(input_id);
    return it == associations_.end() ? "" : it->second;
  }

 private:
  std::map<std::string, std::string> associations_;
};

}  // namespace

class MockAudioRendererHost : public AudioRendererHost {
 public:
  MockAudioRendererHost(base::RunLoop* auth_run_loop,
                        int render_process_id,
                        media::AudioManager* audio_manager,
                        AudioMirroringManager* mirroring_manager,
                        MediaStreamManager* media_stream_manager,
                        const std::string& salt)
      : AudioRendererHost(render_process_id,
                          audio_manager,
                          mirroring_manager,
                          media_stream_manager,
                          salt),
        shared_memory_length_(0),
        auth_run_loop_(auth_run_loop) {
    set_render_frame_id_validate_function_for_testing(&ValidateRenderFrameId);
  }

  // A list of mock methods.
  MOCK_METHOD4(OnDeviceAuthorized,
               void(int stream_id,
                    media::OutputDeviceStatus device_status,
                    const media::AudioParameters& output_params,
                    const std::string& matched_device_id));
  MOCK_METHOD2(WasNotifiedOfCreation, void(int stream_id, int length));
  MOCK_METHOD1(WasNotifiedOfError, void(int stream_id));

  void ShutdownForBadMessage() override { bad_msg_count++; }

  int bad_msg_count = 0;

 private:
  virtual ~MockAudioRendererHost() {
    // Make sure all audio streams have been deleted.
    EXPECT_TRUE(delegates_.empty());
  }

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  // Note: this means that file descriptors won't be duplicated,
  // leading to double-close errors from SyncSocket.
  // See crbug.com/647659.
  bool Send(IPC::Message* message) override {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockAudioRendererHost, *message)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyDeviceAuthorized,
                          OnNotifyDeviceAuthorized)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated,
                          OnNotifyStreamCreated)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamError, OnNotifyStreamError)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  void OnNotifyDeviceAuthorized(int stream_id,
                                media::OutputDeviceStatus device_status,
                                const media::AudioParameters& output_params,
                                const std::string& matched_device_id) {
    // Make sure we didn't leak a raw device id.
    EXPECT_TRUE(IsValidDeviceId(matched_device_id));

    OnDeviceAuthorized(stream_id, device_status, output_params,
                       matched_device_id);
    auth_run_loop_->Quit();
  }

  void OnNotifyStreamCreated(
      int stream_id,
      base::SharedMemoryHandle handle,
      base::SyncSocket::TransitDescriptor socket_descriptor,
      uint32_t length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, false));
    CHECK(shared_memory_->Map(length));
    CHECK(shared_memory_->memory());
    shared_memory_length_ = length;

    // Create the SyncSocket using the handle.
    base::SyncSocket::Handle sync_socket_handle =
        base::SyncSocket::UnwrapHandle(socket_descriptor);
    sync_socket_.reset(new base::SyncSocket(sync_socket_handle));

    // And then delegate the call to the mock method.
    WasNotifiedOfCreation(stream_id, length);
  }

  void OnNotifyStreamError(int stream_id) { WasNotifiedOfError(stream_id); }

  std::unique_ptr<base::SharedMemory> shared_memory_;
  std::unique_ptr<base::SyncSocket> sync_socket_;
  uint32_t shared_memory_length_;
  base::RunLoop* auth_run_loop_;  // Used to wait for authorization.

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererHost);
};

class AudioRendererHostTest : public testing::Test {
 public:
  AudioRendererHostTest()
      : log_factory(base::MakeUnique<media::FakeAudioLogFactory>()),
        audio_manager_(base::MakeUnique<FakeAudioManagerWithAssociations>(
            base::ThreadTaskRunnerHandle::Get(),
            log_factory.get())),
        render_process_host_(&browser_context_, &auth_run_loop_) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    host_ = new MockAudioRendererHost(
        &auth_run_loop_, render_process_host_.GetID(), audio_manager_.get(),
        &mirroring_manager_, media_stream_manager_.get(), kSalt);

    // Simulate IPC channel connected.
    host_->set_peer_process_for_testing(base::Process::Current());
  }

  ~AudioRendererHostTest() override {
    // Simulate closing the IPC channel and give the audio thread time to close
    // the underlying streams.
    host_->OnChannelClosing();
    SyncWithAudioThread();
    // To correctly clean up the audio manager, we first put it in a
    // ScopedAudioManagerPtr. It will immediately destruct, cleaning up the
    // audio manager correctly.
    media::ScopedAudioManagerPtr(audio_manager_.release());

    // Release the reference to the mock object.  The object will be destructed
    // on message_loop_.
    host_ = nullptr;
  }

 protected:
  void OverrideDevicePermissions(bool has_permissions) {
    host_->OverrideDevicePermissionsForTesting(has_permissions);
  }

  std::string GetNondefaultIdExpectedToPassPermissionsCheck() {
    std::string nondefault_id;

    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(
            [](std::string* out, const MediaDeviceEnumeration& result) {
              // Index 0 is default, so use 1. Always exists because we use
              // fake devices.
              CHECK(result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]
                        .size() > 1)
                  << "Expected to have a nondefault device.";
              *out = result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT][1]
                         .device_id;
            },
            base::Unretained(&nondefault_id)));

    // Make sure nondefault_id is set before returning.
    base::RunLoop().RunUntilIdle();

    return nondefault_id;
  }

  std::string GetNondefaultInputId() {
    std::string nondefault_id;

    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::Bind(
            // Index 0 is default, so use 1. Always exists because we use
            // fake devices.
            [](std::string* out, const MediaDeviceEnumeration& result) {
              CHECK(result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT]
                        .size() > 1)
                  << "Expected to have a nondefault device.";
              *out = result[MediaDeviceType::MEDIA_DEVICE_TYPE_AUDIO_INPUT][1]
                         .device_id;
            },
            base::Unretained(&nondefault_id)));

    base::RunLoop().RunUntilIdle();

    return nondefault_id;
  }

  void Create() {
    Create(kDefaultDeviceId, url::Origin(GURL(kSecurityOrigin)), true, true);
  }

  void Create(const std::string& device_id,
              const url::Origin& security_origin,
              bool wait_for_auth,
              bool expect_onauthorized) {
    media::OutputDeviceStatus expected_device_status =
        device_id == kDefaultDeviceId ||
                device_id ==
                    MediaStreamManager::GetHMACForMediaDeviceID(
                        kSalt, url::Origin(GURL(kSecurityOrigin)),
                        GetNondefaultIdExpectedToPassPermissionsCheck())
            ? media::OUTPUT_DEVICE_STATUS_OK
            : device_id == kBadDeviceId
                  ? media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED
                  : media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND;

    if (expect_onauthorized)
      EXPECT_CALL(*host_.get(),
                  OnDeviceAuthorized(kStreamId, expected_device_status, _, _));

    if (expected_device_status == media::OUTPUT_DEVICE_STATUS_OK) {
      EXPECT_CALL(*host_.get(), WasNotifiedOfCreation(kStreamId, _));
      EXPECT_CALL(mirroring_manager_, AddDiverter(render_process_host_.GetID(),
                                                  kRenderFrameId, NotNull()))
          .RetiresOnSaturation();
    }

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    media::AudioParameters params(
        media::AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 16,
        media::AudioParameters::kAudioCDSampleRate / 10);
    int session_id = 0;

    host_->OnRequestDeviceAuthorization(kStreamId, kRenderFrameId, session_id,
                                        device_id, security_origin);
    if (wait_for_auth)
      auth_run_loop_.Run();

    if (!wait_for_auth ||
        expected_device_status == media::OUTPUT_DEVICE_STATUS_OK)
      host_->OnCreateStream(kStreamId, kRenderFrameId, params);

    if (expected_device_status == media::OUTPUT_DEVICE_STATUS_OK)
      // At some point in the future, a corresponding RemoveDiverter() call must
      // be made.
      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()))
          .RetiresOnSaturation();
    SyncWithAudioThread();
  }

  void RequestDeviceAuthorizationWithBadOrigin(const std::string& device_id) {
    int session_id = 0;
    host_->OnRequestDeviceAuthorization(kStreamId, kRenderFrameId, session_id,
                                        device_id,
                                        url::Origin(GURL(kBadSecurityOrigin)));
    SyncWithAudioThread();
  }

  void CreateWithoutWaitingForAuth(const std::string& device_id) {
    Create(device_id, url::Origin(GURL(kSecurityOrigin)), false, false);
  }

  void CreateWithInvalidRenderFrameId() {
    // When creating a stream with an invalid render frame ID, the host will
    // reply with a stream error message.
    EXPECT_CALL(*host_, WasNotifiedOfError(kStreamId));

    // However, validation does not block stream creation, so these method calls
    // might be made:
    EXPECT_CALL(*host_, WasNotifiedOfCreation(kStreamId, _)).Times(AtLeast(0));
    EXPECT_CALL(mirroring_manager_, AddDiverter(_, _, _)).Times(AtLeast(0));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(_)).Times(AtLeast(0));

    // Provide a seemingly-valid render frame ID; and it should be rejected when
    // AudioRendererHost calls ValidateRenderFrameId().
    const int kInvalidRenderFrameId = kRenderFrameId + 1;
    const media::AudioParameters params(
        media::AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 16,
        media::AudioParameters::kAudioCDSampleRate / 10);
    host_->OnCreateStream(kStreamId, kInvalidRenderFrameId, params);
    base::RunLoop().RunUntilIdle();
  }

  void CreateUnifiedStream(const url::Origin& security_origin) {
    std::string output_id = GetNondefaultIdExpectedToPassPermissionsCheck();
    std::string input_id = GetNondefaultInputId();
    std::string hashed_output_id = MediaStreamManager::GetHMACForMediaDeviceID(
        kSalt, url::Origin(GURL(kSecurityOrigin)), output_id);
    // Set up association between input and output so that the output
    // device gets selected when using session id:
    audio_manager_->CreateDeviceAssociation(input_id, output_id);
    int session_id = media_stream_manager_->audio_input_device_manager()->Open(
        StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE, "Fake input device",
                         input_id));
    base::RunLoop().RunUntilIdle();

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    media::AudioParameters params(
        media::AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 16,
        media::AudioParameters::kAudioCDSampleRate / 10);

    EXPECT_CALL(*host_.get(),
                OnDeviceAuthorized(kStreamId, media::OUTPUT_DEVICE_STATUS_OK, _,
                                   hashed_output_id))
        .Times(1);
    EXPECT_CALL(*host_.get(), WasNotifiedOfCreation(kStreamId, _));
    EXPECT_CALL(mirroring_manager_, AddDiverter(render_process_host_.GetID(),
                                                kRenderFrameId, NotNull()))
        .RetiresOnSaturation();
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()))
        .RetiresOnSaturation();

    host_->OnRequestDeviceAuthorization(kStreamId, kRenderFrameId, session_id,
                                        /*device id*/ std::string(),
                                        security_origin);

    auth_run_loop_.Run();

    host_->OnCreateStream(kStreamId, kRenderFrameId, params);

    SyncWithAudioThread();
  }

  void Close() {
    // Send a message to AudioRendererHost to tell it we want to close the
    // stream.
    host_->OnCloseStream(kStreamId);
    SyncWithAudioThread();
  }

  void Play() {
    host_->OnPlayStream(kStreamId);
    SyncWithAudioThread();
  }

  void Pause() {
    host_->OnPauseStream(kStreamId);
    SyncWithAudioThread();
  }

  void SetVolume(double volume) {
    host_->OnSetVolume(kStreamId, volume);
    SyncWithAudioThread();
  }

  void SimulateError() {
    EXPECT_EQ(1u, host_->delegates_.size())
        << "Calls Create() before calling this method";

    // Expect an error signal sent through IPC.
    EXPECT_CALL(*host_.get(), WasNotifiedOfError(kStreamId));

    // Simulate an error sent from the audio device.
    host_->OnStreamError(kStreamId);
    SyncWithAudioThread();

    // Expect the audio stream record is removed.
    EXPECT_EQ(0u, host_->delegates_.size());
  }

  // SyncWithAudioThread() waits until all pending tasks on the audio thread
  // are executed while also processing pending task in message_loop_ on the
  // current thread. It is used to synchronize with the audio thread when we are
  // closing an audio stream.
  void SyncWithAudioThread() {
    base::RunLoop().RunUntilIdle();

    base::RunLoop run_loop;
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE, media::BindToCurrentLoop(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void AssertBadMsgReported() {
    // Bad messages can be reported either directly to the RPH or through the
    // ARH, so we check both of them.
    EXPECT_EQ(render_process_host_.bad_msg_count() + host_->bad_msg_count, 1);
  }

 private:
  // MediaStreamManager uses a DestructionObserver, so it must outlive the
  // TestBrowserThreadBundle.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserThreadBundle thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<media::FakeAudioLogFactory> log_factory;
  std::unique_ptr<FakeAudioManagerWithAssociations> audio_manager_;
  MockAudioMirroringManager mirroring_manager_;
  base::RunLoop auth_run_loop_;
  MockRenderProcessHostWithSignaling render_process_host_;
  scoped_refptr<MockAudioRendererHost> host_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHostTest);
};

TEST_F(AudioRendererHostTest, CreateAndClose) {
  Create();
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreateAndShutdown) {
  Create();
}

TEST_F(AudioRendererHostTest, CreatePlayAndClose) {
  Create();
  Play();
  Close();
}

TEST_F(AudioRendererHostTest, CreatePlayPauseAndClose) {
  Create();
  Play();
  Pause();
  Close();
}

TEST_F(AudioRendererHostTest, SetVolume) {
  Create();
  SetVolume(0.5);
  Play();
  Pause();
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayAndShutdown) {
  Create();
  Play();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayPauseAndShutdown) {
  Create();
  Play();
  Pause();
}

TEST_F(AudioRendererHostTest, SimulateError) {
  Create();
  Play();
  SimulateError();
}

// Simulate the case when an error is generated on the browser process,
// the audio device is closed but the render process try to close the
// audio stream again.
TEST_F(AudioRendererHostTest, SimulateErrorAndClose) {
  Create();
  Play();
  SimulateError();
  Close();
}

TEST_F(AudioRendererHostTest, CreateUnifiedStreamAndClose) {
  CreateUnifiedStream(url::Origin(GURL(kSecurityOrigin)));
  Close();
}

TEST_F(AudioRendererHostTest, CreateUnauthorizedDevice) {
  Create(kBadDeviceId, url::Origin(GURL(kSecurityOrigin)), true, true);
  Close();
}

TEST_F(AudioRendererHostTest, CreateAuthorizedDevice) {
  OverrideDevicePermissions(true);
  std::string id = GetNondefaultIdExpectedToPassPermissionsCheck();
  std::string hashed_id = MediaStreamManager::GetHMACForMediaDeviceID(
      kSalt, url::Origin(GURL(kSecurityOrigin)), id);
  Create(hashed_id, url::Origin(GURL(kSecurityOrigin)), true, true);
  Close();
}

TEST_F(AudioRendererHostTest, CreateDeviceWithAuthorizationPendingIsError) {
  CreateWithoutWaitingForAuth(kBadDeviceId);
  Close();
  AssertBadMsgReported();
}

TEST_F(AudioRendererHostTest, CreateDeviceWithBadSecurityOrigin) {
  RequestDeviceAuthorizationWithBadOrigin(kNondefaultDeviceId);
  Close();
  AssertBadMsgReported();
}

TEST_F(AudioRendererHostTest, CreateInvalidDevice) {
  Create(kInvalidDeviceId, url::Origin(GURL(kSecurityOrigin)), true, true);
  Close();
}

TEST_F(AudioRendererHostTest, CreateFailsForInvalidRenderFrame) {
  CreateWithInvalidRenderFrameId();
  Close();
}

// TODO(hclam): Add tests for data conversation in low latency mode.

}  // namespace content
