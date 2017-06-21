// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/fake_audio_input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using base::WaitableEvent;

namespace media {

static const int kSampleRate = AudioParameters::kAudioCDSampleRate;
static const int kBitsPerSample = 16;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kSamplesPerPacket = kSampleRate / 10;

// AudioInputController will poll once every second, so wait at most a bit
// more than that for the callbacks.
static const int kOnMuteWaitTimeoutMs = 1500;

ACTION_P(QuitRunLoop, run_loop) {
  run_loop->QuitWhenIdle();
}

// Posts base::MessageLoop::QuitWhenIdleClosure() on specified message loop
// after a certain number of calls given by |limit|.
ACTION_P3(CheckCountAndPostQuitTask, count, limit, loop_or_proxy) {
  if (++*count >= limit) {
    loop_or_proxy->PostTask(FROM_HERE,
                            base::MessageLoop::QuitWhenIdleClosure());
  }
}

// Closes AudioOutputController synchronously.
static void CloseAudioController(AudioInputController* controller) {
  controller->Close(base::MessageLoop::QuitWhenIdleClosure());
  base::RunLoop().Run();
}

class MockAudioInputControllerEventHandler
    : public AudioInputController::EventHandler {
 public:
  MockAudioInputControllerEventHandler() {}

  MOCK_METHOD1(OnCreated, void(AudioInputController* controller));
  MOCK_METHOD2(OnError, void(AudioInputController* controller,
                             AudioInputController::ErrorCode error_code));
  MOCK_METHOD2(OnLog,
               void(AudioInputController* controller,
                    const std::string& message));
  MOCK_METHOD2(OnMuted, void(AudioInputController* controller, bool is_muted));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputControllerEventHandler);
};

class MockSyncWriter : public AudioInputController::SyncWriter {
 public:
  MockSyncWriter() {}

  MOCK_METHOD4(Write,
               void(const AudioBus* data,
                    double volume,
                    bool key_pressed,
                    uint32_t hardware_delay_bytes));
  MOCK_METHOD0(Close, void());
};

// Test fixture.
class AudioInputControllerTest : public testing::Test {
 public:
  AudioInputControllerTest()
      : suspend_event_(WaitableEvent::ResetPolicy::AUTOMATIC,
                       WaitableEvent::InitialState::NOT_SIGNALED) {
    audio_manager_ =
        AudioManager::CreateForTesting(base::MakeUnique<AudioThreadImpl>());
  }
  ~AudioInputControllerTest() override { audio_manager_->Shutdown(); }

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner() const {
    return audio_manager_->GetTaskRunner();
  }

  void SuspendAudioThread() {
    audio_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioInputControllerTest::WaitForResume,
                                  base::Unretained(this)));
  }

  void ResumeAudioThread() { suspend_event_.Signal(); }

 private:
  void WaitForResume() { suspend_event_.Wait(); }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<AudioManager> audio_manager_;
  WaitableEvent suspend_event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputControllerTest);
};

// Test AudioInputController for create and close without recording audio.
TEST_F(AudioInputControllerTest, CreateAndClose) {
  base::RunLoop run_loop;

  MockAudioInputControllerEventHandler event_handler;
  MockSyncWriter sync_writer;
  scoped_refptr<AudioInputController> controller;

  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  SuspendAudioThread();
  controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, &sync_writer, nullptr, params,
      AudioDeviceDescription::kDefaultDeviceId, false);
  ASSERT_TRUE(controller.get());
  EXPECT_CALL(event_handler, OnCreated(controller.get())).Times(Exactly(1));
  EXPECT_CALL(event_handler, OnLog(controller.get(), _)).Times(Exactly(3));
  EXPECT_CALL(sync_writer, Close()).Times(Exactly(1));
  ResumeAudioThread();

  CloseAudioController(controller.get());
}

// Test a normal call sequence of create, record and close.
TEST_F(AudioInputControllerTest, RecordAndClose) {
  MockAudioInputControllerEventHandler event_handler;
  MockSyncWriter sync_writer;
  int count = 0;

  // OnCreated() will be called once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .Times(Exactly(1));

  // Write() should be called ten times.
  EXPECT_CALL(sync_writer, Write(NotNull(), _, _, _))
      .Times(AtLeast(10))
      .WillRepeatedly(
          CheckCountAndPostQuitTask(&count, 10, message_loop_.task_runner()));

  EXPECT_CALL(event_handler, OnLog(_, _)).Times(AnyNumber());
  EXPECT_CALL(sync_writer, Close()).Times(Exactly(1));

  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  // Creating the AudioInputController should render an OnCreated() call.
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, &sync_writer, nullptr, params,
      AudioDeviceDescription::kDefaultDeviceId, false);
  ASSERT_TRUE(controller.get());

  controller->Record();

  // Record and wait until ten Write() callbacks are received.
  base::RunLoop().Run();
  CloseAudioController(controller.get());
}

// Test that AudioInputController rejects insanely large packet sizes.
TEST_F(AudioInputControllerTest, SamplesPerPacketTooLarge) {
  // Create an audio device with a very large packet size.
  MockAudioInputControllerEventHandler event_handler;
  MockSyncWriter sync_writer;

  // OnCreated() shall not be called in this test.
  EXPECT_CALL(event_handler, OnCreated(NotNull())).Times(Exactly(0));

  AudioParameters params(AudioParameters::AUDIO_FAKE,
                         kChannelLayout,
                         kSampleRate,
                         kBitsPerSample,
                         kSamplesPerPacket * 1000);
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, &sync_writer, nullptr, params,
      AudioDeviceDescription::kDefaultDeviceId, false);
  ASSERT_FALSE(controller.get());
}

// Test calling AudioInputController::Close multiple times.
TEST_F(AudioInputControllerTest, CloseTwice) {
  MockAudioInputControllerEventHandler event_handler;
  MockSyncWriter sync_writer;

  // OnCreated() will be called only once.
  EXPECT_CALL(event_handler, OnCreated(NotNull())).Times(Exactly(1));
  EXPECT_CALL(event_handler, OnLog(_, _)).Times(AnyNumber());
  // This callback should still only be called once.
  EXPECT_CALL(sync_writer, Close()).Times(Exactly(1));

  AudioParameters params(AudioParameters::AUDIO_FAKE,
                         kChannelLayout,
                         kSampleRate,
                         kBitsPerSample,
                         kSamplesPerPacket);
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, &sync_writer, nullptr, params,
      AudioDeviceDescription::kDefaultDeviceId, false);
  ASSERT_TRUE(controller.get());

  controller->Record();

  controller->Close(base::MessageLoop::QuitWhenIdleClosure());
  base::RunLoop().Run();

  controller->Close(base::MessageLoop::QuitWhenIdleClosure());
  base::RunLoop().Run();
}

namespace {
void RunLoopWithTimeout(base::RunLoop* run_loop, base::TimeDelta timeout) {
  base::OneShotTimer timeout_timer;
  timeout_timer.Start(FROM_HERE, timeout, run_loop->QuitClosure());
  run_loop->Run();
}
}

// Test that AudioInputController sends OnMute callbacks properly.
TEST_F(AudioInputControllerTest, TestOnmutedCallbackInitiallyUnmuted) {
  const auto timeout = base::TimeDelta::FromMilliseconds(kOnMuteWaitTimeoutMs);
  MockAudioInputControllerEventHandler event_handler;
  MockSyncWriter sync_writer;
  scoped_refptr<AudioInputController> controller;
  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  base::RunLoop unmute_run_loop;
  base::RunLoop mute_run_loop;
  base::RunLoop setup_run_loop;
  EXPECT_CALL(event_handler, OnCreated(_)).Times(Exactly(1));
  EXPECT_CALL(event_handler, OnLog(_, _)).Times(Exactly(3));
  EXPECT_CALL(sync_writer, Close()).Times(Exactly(1));
  EXPECT_CALL(event_handler, OnMuted(_, true))
      .WillOnce(InvokeWithoutArgs([&] { mute_run_loop.Quit(); }));
  EXPECT_CALL(event_handler, OnMuted(_, false))
      .WillOnce(InvokeWithoutArgs([&] { unmute_run_loop.Quit(); }));

  FakeAudioInputStream::SetGlobalMutedState(false);
  controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, &sync_writer, nullptr, params,
      AudioDeviceDescription::kDefaultDeviceId, false);
  ASSERT_TRUE(controller.get());
  setup_run_loop.RunUntilIdle();

  FakeAudioInputStream::SetGlobalMutedState(true);
  RunLoopWithTimeout(&mute_run_loop, timeout);
  FakeAudioInputStream::SetGlobalMutedState(false);
  RunLoopWithTimeout(&unmute_run_loop, timeout);

  CloseAudioController(controller.get());
}

TEST_F(AudioInputControllerTest, TestOnmutedCallbackInitiallyMuted) {
  const auto timeout = base::TimeDelta::FromMilliseconds(kOnMuteWaitTimeoutMs);
  MockAudioInputControllerEventHandler event_handler;
  MockSyncWriter sync_writer;
  scoped_refptr<AudioInputController> controller;
  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  base::RunLoop unmute_run_loop;
  base::RunLoop setup_run_loop;
  EXPECT_CALL(event_handler, OnCreated(_)).Times(Exactly(1));
  EXPECT_CALL(event_handler, OnLog(_, _)).Times(Exactly(3));
  EXPECT_CALL(sync_writer, Close()).Times(Exactly(1));
  EXPECT_CALL(event_handler, OnMuted(_, true))
      .WillOnce(InvokeWithoutArgs([&] { setup_run_loop.QuitWhenIdle(); }));
  EXPECT_CALL(event_handler, OnMuted(_, false))
      .WillOnce(InvokeWithoutArgs([&] { unmute_run_loop.Quit(); }));

  FakeAudioInputStream::SetGlobalMutedState(true);
  controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, &sync_writer, nullptr, params,
      AudioDeviceDescription::kDefaultDeviceId, false);
  ASSERT_TRUE(controller.get());
  RunLoopWithTimeout(&setup_run_loop, timeout);

  FakeAudioInputStream::SetGlobalMutedState(false);
  RunLoopWithTimeout(&unmute_run_loop, timeout);

  CloseAudioController(controller.get());
}

}  // namespace media
