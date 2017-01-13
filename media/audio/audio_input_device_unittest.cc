// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/sync_socket.h"
#include "media/audio/audio_input_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CancelableSyncSocket;
using base::SharedMemory;
using base::SyncSocket;
using testing::_;
using testing::DoAll;
using testing::Invoke;

namespace media {

namespace {

class MockAudioInputIPC : public AudioInputIPC {
 public:
  MockAudioInputIPC() {}
  ~MockAudioInputIPC() override {}

  MOCK_METHOD5(CreateStream,
               void(AudioInputIPCDelegate* delegate,
                    int session_id,
                    const AudioParameters& params,
                    bool automatic_gain_control,
                    uint32_t total_segments));
  MOCK_METHOD0(RecordStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD0(CloseStream, void());
};

class MockCaptureCallback : public AudioCapturerSource::CaptureCallback {
 public:
  MockCaptureCallback() {}
  ~MockCaptureCallback() override {}

  MOCK_METHOD0(OnCaptureStarted, void());
  MOCK_METHOD4(Capture,
               void(const AudioBus* audio_source,
                    int audio_delay_milliseconds,
                    double volume,
                    bool key_pressed));

  MOCK_METHOD1(OnCaptureError, void(const std::string& message));
};

// Used to terminate a loop from a different thread than the loop belongs to.
// |task_runner| should be a SingleThreadTaskRunner.
ACTION_P(QuitLoop, task_runner) {
  task_runner->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

}  // namespace.

// Regular construction.
TEST(AudioInputDeviceTest, Noop) {
  base::MessageLoopForIO io_loop;
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(
      new AudioInputDevice(base::WrapUnique(input_ipc), io_loop.task_runner()));
}

ACTION_P(ReportStateChange, device) {
  static_cast<AudioInputIPCDelegate*>(device)->OnError();
}

// Verify that we get an OnCaptureError() callback if CreateStream fails.
TEST(AudioInputDeviceTest, FailToCreateStream) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 16, 480);

  base::MessageLoopForIO io_loop;
  MockCaptureCallback callback;
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(
      new AudioInputDevice(base::WrapUnique(input_ipc), io_loop.task_runner()));
  device->Initialize(params, &callback, 1);
  device->Start();
  EXPECT_CALL(*input_ipc, CreateStream(_, _, _, _, _))
      .WillOnce(ReportStateChange(device.get()));
  EXPECT_CALL(callback, OnCaptureError(_))
      .WillOnce(QuitLoop(io_loop.task_runner()));
  base::RunLoop().Run();
}

ACTION_P5(ReportOnStreamCreated, device, handle, socket, length, segments) {
  static_cast<AudioInputIPCDelegate*>(device)->OnStreamCreated(
      handle, socket, length, segments);
}

TEST(AudioInputDeviceTest, CreateStream) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 16, 480);
  SharedMemory shared_memory;
  CancelableSyncSocket browser_socket;
  CancelableSyncSocket renderer_socket;

  const int memory_size = sizeof(AudioInputBufferParameters) +
                          AudioBus::CalculateMemorySize(params);

  ASSERT_TRUE(shared_memory.CreateAndMapAnonymous(memory_size));
  memset(shared_memory.memory(), 0xff, memory_size);

  ASSERT_TRUE(
      CancelableSyncSocket::CreatePair(&browser_socket, &renderer_socket));
  SyncSocket::TransitDescriptor audio_device_socket_descriptor;
  ASSERT_TRUE(renderer_socket.PrepareTransitDescriptor(
      base::GetCurrentProcessHandle(), &audio_device_socket_descriptor));
  base::SharedMemoryHandle duplicated_memory_handle;
  ASSERT_TRUE(shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                           &duplicated_memory_handle));

  base::MessageLoopForIO io_loop;
  MockCaptureCallback callback;
  MockAudioInputIPC* input_ipc = new MockAudioInputIPC();
  scoped_refptr<AudioInputDevice> device(
      new AudioInputDevice(base::WrapUnique(input_ipc), io_loop.task_runner()));
  device->Initialize(params, &callback, 1);
  device->Start();

  EXPECT_CALL(*input_ipc, CreateStream(_, _, _, _, _))
      .WillOnce(ReportOnStreamCreated(
          device.get(), duplicated_memory_handle,
          SyncSocket::UnwrapHandle(audio_device_socket_descriptor), memory_size,
          1));
  EXPECT_CALL(*input_ipc, RecordStream());
  EXPECT_CALL(callback, OnCaptureStarted())
      .WillOnce(QuitLoop(io_loop.task_runner()));
  base::RunLoop().Run();
}
}  // namespace media.
