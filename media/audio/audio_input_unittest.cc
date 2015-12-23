// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/environment.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_unittest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// This class allows to find out if the callbacks are occurring as
// expected and if any error has been reported.
class TestInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  explicit TestInputCallback()
      : callback_count_(0),
        had_error_(0) {
  }
  void OnData(AudioInputStream* stream,
              const AudioBus* source,
              uint32_t hardware_delay_bytes,
              double volume) override {
    ++callback_count_;
  }
  void OnError(AudioInputStream* stream) override { ++had_error_; }
  // Returns how many times OnData() has been called.
  int callback_count() const {
    return callback_count_;
  }
  // Returns how many times the OnError callback was called.
  int had_error() const {
    return had_error_;
  }

 private:
  int callback_count_;
  int had_error_;
};

class AudioInputTest : public testing::Test {
  public:
   AudioInputTest() :
      message_loop_(base::MessageLoop::TYPE_UI),
      audio_manager_(AudioManager::CreateForTesting()),
      audio_input_stream_(NULL) {
    // Wait for the AudioManager to finish any initialization on the audio loop.
    base::RunLoop().RunUntilIdle();
  }

  ~AudioInputTest() override { base::RunLoop().RunUntilIdle(); }

 protected:
  bool InputDevicesAvailable() {
    return audio_manager_->HasAudioInputDevices();
  }

  void MakeAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::Bind(&AudioInputTest::MakeAudioInputStream,
                   base::Unretained(this)));
  }

  void CloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::Bind(&AudioInputStream::Close,
                   base::Unretained(audio_input_stream_)));
    audio_input_stream_ = NULL;
  }

  void OpenAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::Bind(&AudioInputTest::OpenAndClose,
                   base::Unretained(this)));
  }

  void OpenStopAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::Bind(&AudioInputTest::OpenStopAndClose,
                   base::Unretained(this)));
  }

  void OpenAndStartAudioInputStreamOnAudioThread(
      AudioInputStream::AudioInputCallback* sink) {
    RunOnAudioThread(
        base::Bind(&AudioInputTest::OpenAndStart,
                   base::Unretained(this),
                   sink));
  }

  void StopAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::Bind(&AudioInputTest::StopAndClose,
                   base::Unretained(this)));
  }

  void MakeAudioInputStream() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    AudioParameters params = audio_manager_->GetInputStreamParameters(
        AudioManagerBase::kDefaultDeviceId);
    audio_input_stream_ = audio_manager_->MakeAudioInputStream(params,
        AudioManagerBase::kDefaultDeviceId);
    EXPECT_TRUE(audio_input_stream_);
  }

  void OpenAndClose() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    EXPECT_TRUE(audio_input_stream_->Open());
    audio_input_stream_->Close();
    audio_input_stream_ = NULL;
  }

  void OpenAndStart(AudioInputStream::AudioInputCallback* sink) {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    EXPECT_TRUE(audio_input_stream_->Open());
    audio_input_stream_->Start(sink);
  }

  void OpenStopAndClose() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    EXPECT_TRUE(audio_input_stream_->Open());
    audio_input_stream_->Stop();
    audio_input_stream_->Close();
    audio_input_stream_ = NULL;
  }

  void StopAndClose() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    audio_input_stream_->Stop();
    audio_input_stream_->Close();
    audio_input_stream_ = NULL;
  }

  // Synchronously runs the provided callback/closure on the audio thread.
  void RunOnAudioThread(const base::Closure& closure) {
    if (!audio_manager_->GetTaskRunner()->BelongsToCurrentThread()) {
      base::WaitableEvent event(false, false);
      audio_manager_->GetTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&AudioInputTest::RunOnAudioThreadImpl,
                     base::Unretained(this),
                     closure,
                     &event));
      event.Wait();
    } else {
      closure.Run();
    }
  }

  void RunOnAudioThreadImpl(const base::Closure& closure,
                            base::WaitableEvent* event) {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    closure.Run();
    event->Signal();
  }

  base::MessageLoop message_loop_;
  scoped_ptr<AudioManager> audio_manager_;
  AudioInputStream* audio_input_stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputTest);
};

// Test create and close of an AudioInputStream without recording audio.
TEST_F(AudioInputTest, CreateAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();
  CloseAudioInputStreamOnAudioThread();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// This test is failing on ARM linux: http://crbug.com/238490
#define MAYBE_OpenAndClose DISABLED_OpenAndClose
#else
#define MAYBE_OpenAndClose OpenAndClose
#endif
// Test create, open and close of an AudioInputStream without recording audio.
TEST_F(AudioInputTest, MAYBE_OpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();
  OpenAndCloseAudioInputStreamOnAudioThread();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// This test is failing on ARM linux: http://crbug.com/238490
#define MAYBE_OpenStopAndClose DISABLED_OpenStopAndClose
#else
#define MAYBE_OpenStopAndClose OpenStopAndClose
#endif
// Test create, open, stop and close of an AudioInputStream without recording.
TEST_F(AudioInputTest, MAYBE_OpenStopAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();
  OpenStopAndCloseAudioInputStreamOnAudioThread();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// This test is failing on ARM linux: http://crbug.com/238490
#define MAYBE_Record DISABLED_Record
#else
#define MAYBE_Record Record
#endif
// Test a normal recording sequence using an AudioInputStream.
// Very simple test which starts capturing during half a second and verifies
// that recording starts.
TEST_F(AudioInputTest, MAYBE_Record) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();

  TestInputCallback test_callback;
  OpenAndStartAudioInputStreamOnAudioThread(&test_callback);

  message_loop_.PostDelayedTask(FROM_HERE,
                                base::MessageLoop::QuitWhenIdleClosure(),
                                base::TimeDelta::FromMilliseconds(500));
  message_loop_.Run();
  EXPECT_GE(test_callback.callback_count(), 2);
  EXPECT_FALSE(test_callback.had_error());

  StopAndCloseAudioInputStreamOnAudioThread();
}

}  // namespace media
