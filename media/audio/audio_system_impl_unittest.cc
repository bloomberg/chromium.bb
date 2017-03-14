// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/mock_audio_manager.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char* kNonDefaultDeviceId = "non-default-device-id";
}
namespace media {

class AudioSystemImplTest : public testing::TestWithParam<bool> {
 public:
  AudioSystemImplTest()
      : use_audio_thread_(GetParam()),
        audio_thread_("AudioSystemThread"),
        input_params_(AudioParameters::AUDIO_PCM_LINEAR,
                      CHANNEL_LAYOUT_MONO,
                      AudioParameters::kTelephoneSampleRate,
                      16,
                      AudioParameters::kTelephoneSampleRate / 10),
        output_params_(AudioParameters::AUDIO_PCM_LINEAR,
                       CHANNEL_LAYOUT_MONO,
                       AudioParameters::kTelephoneSampleRate,
                       16,
                       AudioParameters::kTelephoneSampleRate / 20),
        default_output_params_(AudioParameters::AUDIO_PCM_LINEAR,
                               CHANNEL_LAYOUT_MONO,
                               AudioParameters::kTelephoneSampleRate,
                               16,
                               AudioParameters::kTelephoneSampleRate / 30) {
    if (use_audio_thread_) {
      audio_thread_.StartAndWaitForTesting();
      audio_manager_.reset(
          new media::MockAudioManager(audio_thread_.task_runner()));
    } else {
      audio_manager_.reset(new media::MockAudioManager(
          base::ThreadTaskRunnerHandle::Get().get()));
    }

    audio_manager_->SetInputStreamParameters(input_params_);
    audio_manager_->SetOutputStreamParameters(output_params_);
    audio_manager_->SetDefaultOutputStreamParameters(default_output_params_);

    audio_system_ = media::AudioSystemImpl::Create(audio_manager_.get());
    EXPECT_EQ(AudioSystem::Get(), audio_system_.get());
  }

  ~AudioSystemImplTest() override {
    // Deleting |audio_manager_| on its thread.
    audio_system_.reset();
    EXPECT_EQ(AudioSystem::Get(), nullptr);
    audio_manager_.reset();
    audio_thread_.Stop();
  }

  void OnAudioParams(const AudioParameters& expected,
                     const AudioParameters& received) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    EXPECT_EQ(expected.AsHumanReadableString(),
              received.AsHumanReadableString());
    AudioParametersReceived();
  }

  void OnHasInputDevices(bool result) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    HasInputDevicesCallback(result);
  }

  void WaitForCallback() {
    if (!use_audio_thread_) {
      base::RunLoop().RunUntilIdle();
      return;
    }
    WaitableMessageLoopEvent event;
    audio_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing), event.GetClosure());
    // Runs the loop and waits for the |audio_thread_| to call event's closure,
    // which means AudioSystem reply containing device parameters is already
    // queued on the main thread.
    event.RunAndWait();
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD0(AudioParametersReceived, void(void));
  MOCK_METHOD1(HasInputDevicesCallback, void(bool));

 protected:
  base::MessageLoop message_loop_;
  base::ThreadChecker thread_checker_;
  bool use_audio_thread_;
  base::Thread audio_thread_;
  MockAudioManager::UniquePtr audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  AudioParameters input_params_;
  AudioParameters output_params_;
  AudioParameters default_output_params_;
};

TEST_P(AudioSystemImplTest, GetInputStreamParameters) {
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetInputStreamParameters(
      media::AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 input_params_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetInputStreamParametersNoDevice) {
  audio_manager_->SetHasInputDevices(false);
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetInputStreamParameters(
      media::AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 media::AudioParameters()));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetStreamParameters) {
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetOutputStreamParameters(
      kNonDefaultDeviceId, base::Bind(&AudioSystemImplTest::OnAudioParams,
                                      base::Unretained(this), output_params_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetDefaultOutputStreamParameters) {
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetOutputStreamParameters(
      media::AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 default_output_params_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetOutputStreamParametersNoDevice) {
  audio_manager_->SetHasOutputDevices(false);
  EXPECT_CALL(*this, AudioParametersReceived()).Times(2);

  audio_system_->GetOutputStreamParameters(
      media::AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 media::AudioParameters()));
  WaitForCallback();

  audio_system_->GetOutputStreamParameters(
      kNonDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 media::AudioParameters()));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, HasInputDevices) {
  EXPECT_CALL(*this, HasInputDevicesCallback(true));
  audio_system_->HasInputDevices(base::Bind(
      &AudioSystemImplTest::OnHasInputDevices, base::Unretained(this)));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, HasNoInputDevices) {
  audio_manager_->SetHasInputDevices(false);
  EXPECT_CALL(*this, HasInputDevicesCallback(false));
  audio_system_->HasInputDevices(base::Bind(
      &AudioSystemImplTest::OnHasInputDevices, base::Unretained(this)));
  WaitForCallback();
}

INSTANTIATE_TEST_CASE_P(, AudioSystemImplTest, testing::Values(false, true));

}  // namespace media
