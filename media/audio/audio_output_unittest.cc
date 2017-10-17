// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/memory/aligned_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioOutputTest : public ::testing::Test {
 public:
  AudioOutputTest() {
    audio_manager_ =
        AudioManager::CreateForTesting(base::MakeUnique<TestAudioThread>());
    audio_manager_device_info_ =
        base::MakeUnique<AudioDeviceInfoAccessorForTests>(audio_manager_.get());
    base::RunLoop().RunUntilIdle();
  }
  ~AudioOutputTest() override { audio_manager_->Shutdown(); }

  // Runs message loop for the specified amount of time.
  void RunMessageLoop(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<AudioManager> audio_manager_;
  std::unique_ptr<AudioDeviceInfoAccessorForTests> audio_manager_device_info_;
};

// Test that can it be created and closed.
TEST_F(AudioOutputTest, GetAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, 8000, 16, 256),
      std::string(), AudioManager::LogCallback());
  ASSERT_NE(nullptr, oas);
  oas->Close();
}

// Test that it can be opened and closed.
TEST_F(AudioOutputTest, OpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(audio_manager_device_info_->HasAudioOutputDevices());

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, 8000, 16, 256),
      std::string(), AudioManager::LogCallback());
  ASSERT_NE(nullptr, oas);
  EXPECT_TRUE(oas->Open());
  oas->Close();
}

// This test produces actual audio for .5 seconds on the default wave
// device at 44.1K s/sec.
TEST_F(AudioOutputTest, Play200HzTone44kHz) {
  if (!audio_manager_device_info_->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  uint32_t samples_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_MONO, AudioParameters::kAudioCDSampleRate,
                      16, samples_100_ms),
      std::string(), AudioManager::LogCallback());
  ASSERT_NE(nullptr, oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);

  EXPECT_TRUE(oas->Open());
  oas->SetVolume(1.0);
  oas->Start(&source);
  RunMessageLoop(base::TimeDelta::FromMilliseconds(250));
  oas->Stop();
  oas->Close();

  EXPECT_FALSE(source.errors());
  EXPECT_GT(source.callbacks(), 1);
}

// Test that SetVolume() and GetVolume() work as expected.
TEST_F(AudioOutputTest, VolumeControl) {
  if (!audio_manager_device_info_->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  AudioOutputStream* oas = audio_manager_->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, 8000, 16, 256),
      std::string(), AudioManager::LogCallback());
  ASSERT_NE(nullptr, oas);
  EXPECT_TRUE(oas->Open());

  double volume = 0.0;

  oas->GetVolume(&volume);
  EXPECT_EQ(volume, 1.0);

  oas->SetVolume(0.5);

  oas->GetVolume(&volume);
  EXPECT_LT(volume, 0.51);
  EXPECT_GT(volume, 0.49);
  oas->Stop();

  oas->Close();
}

}  // namespace media
