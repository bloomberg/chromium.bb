// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char* kNonDefaultDeviceId = "non-default-device-id";
}

namespace media {

bool operator==(const AudioDeviceDescription& lhs,
                const AudioDeviceDescription& rhs) {
  return lhs.device_name == rhs.device_name && lhs.unique_id == rhs.unique_id &&
         lhs.group_id == rhs.group_id;
}

// TODO(olka): These are the only tests for AudioSystemHelper. Make sure that
// AudioSystemHelper is tested if AudioSystemImpl goes away.
class AudioSystemImplTest : public testing::TestWithParam<bool> {
 public:
  AudioSystemImplTest()
      : use_audio_thread_(GetParam()),
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
    audio_manager_ = base::MakeUnique<MockAudioManager>(
        base::MakeUnique<TestAudioThread>(use_audio_thread_));
    audio_manager_->SetInputStreamParameters(input_params_);
    audio_manager_->SetOutputStreamParameters(output_params_);
    audio_manager_->SetDefaultOutputStreamParameters(default_output_params_);

    auto get_device_descriptions = [](const AudioDeviceDescriptions* source,
                                      AudioDeviceDescriptions* destination) {
      destination->insert(destination->end(), source->begin(), source->end());
    };

    audio_manager_->SetInputDeviceDescriptionsCallback(
        base::Bind(get_device_descriptions,
                   base::Unretained(&input_device_descriptions_)));
    audio_manager_->SetOutputDeviceDescriptionsCallback(
        base::Bind(get_device_descriptions,
                   base::Unretained(&output_device_descriptions_)));

    audio_system_ = std::make_unique<AudioSystemImpl>(audio_manager_.get());
  }

  ~AudioSystemImplTest() override { audio_manager_->Shutdown(); }

  void OnAudioParams(const base::Optional<AudioParameters>& expected,
                     const base::Optional<AudioParameters>& received) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    if (expected) {
      EXPECT_TRUE(received);
      EXPECT_EQ(expected->AsHumanReadableString(),
                received->AsHumanReadableString());
    } else {
      EXPECT_FALSE(received);
    }
    AudioParametersReceived();
  }

  void OnHasInputDevices(bool result) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    HasInputDevicesCallback(result);
  }

  void OnHasOutputDevices(bool result) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    HasOutputDevicesCallback(result);
  }

  void OnGetDeviceDescriptions(
      const AudioDeviceDescriptions& expected_descriptions,
      AudioDeviceDescriptions descriptions) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    EXPECT_EQ(expected_descriptions, descriptions);
    DeviceDescriptionsReceived();
  }

  void OnInputDeviceInfo(
      const base::Optional<AudioParameters>& expected_input,
      const base::Optional<AudioParameters>& expected_associated_output,
      const std::string& expected_associated_device_id,
      const base::Optional<AudioParameters>& input,
      const base::Optional<AudioParameters>& associated_output,
      const std::string& associated_device_id) {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    if (expected_input) {
      EXPECT_TRUE(input);
      EXPECT_EQ(expected_input->AsHumanReadableString(),
                input->AsHumanReadableString());
    } else {
      EXPECT_FALSE(input);
    }
    if (expected_associated_output) {
      EXPECT_TRUE(associated_output);
      EXPECT_EQ(expected_associated_output->AsHumanReadableString(),
                associated_output->AsHumanReadableString());
    } else {
      EXPECT_FALSE(associated_output);
    }
    EXPECT_EQ(expected_associated_device_id, associated_device_id);
    InputDeviceInfoReceived();
  }

  void WaitForCallback() {
    if (!use_audio_thread_) {
      base::RunLoop().RunUntilIdle();
      return;
    }
    WaitableMessageLoopEvent event;
    audio_manager_->GetTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing), event.GetClosure());
    // Runs the loop and waits for the |audio_thread_| to call event's closure,
    // which means AudioSystem reply containing device parameters is already
    // queued on the main thread.
    event.RunAndWait();
    base::RunLoop().RunUntilIdle();
  }

  // Mocks to verify that AudioSystem replied with an expected callback.
  MOCK_METHOD0(AudioParametersReceived, void(void));
  MOCK_METHOD1(HasInputDevicesCallback, void(bool));
  MOCK_METHOD1(HasOutputDevicesCallback, void(bool));
  MOCK_METHOD0(DeviceDescriptionsReceived, void(void));
  MOCK_METHOD1(AssociatedOutputDeviceIDReceived, void(const std::string&));
  MOCK_METHOD0(InputDeviceInfoReceived, void(void));

 protected:
  base::MessageLoop message_loop_;
  base::ThreadChecker thread_checker_;
  bool use_audio_thread_;
  std::unique_ptr<MockAudioManager> audio_manager_;
  std::unique_ptr<AudioSystem> audio_system_;
  AudioParameters input_params_;
  AudioParameters output_params_;
  AudioParameters default_output_params_;
  AudioDeviceDescriptions input_device_descriptions_;
  AudioDeviceDescriptions output_device_descriptions_;
};

TEST_P(AudioSystemImplTest, GetInputStreamParameters) {
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetInputStreamParameters(
      AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 input_params_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetInputStreamParametersNoDevice) {
  audio_manager_->SetHasInputDevices(false);
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetInputStreamParameters(
      AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 base::Optional<AudioParameters>()));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetOutputStreamParameters) {
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetOutputStreamParameters(
      kNonDefaultDeviceId, base::Bind(&AudioSystemImplTest::OnAudioParams,
                                      base::Unretained(this), output_params_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetDefaultOutputStreamParameters) {
  EXPECT_CALL(*this, AudioParametersReceived());
  audio_system_->GetOutputStreamParameters(
      AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 default_output_params_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetOutputStreamParametersNoDevice) {
  audio_manager_->SetHasOutputDevices(false);
  EXPECT_CALL(*this, AudioParametersReceived()).Times(2);

  audio_system_->GetOutputStreamParameters(
      AudioDeviceDescription::kDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 base::Optional<AudioParameters>()));
  WaitForCallback();

  audio_system_->GetOutputStreamParameters(
      kNonDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnAudioParams, base::Unretained(this),
                 base::Optional<AudioParameters>()));
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

TEST_P(AudioSystemImplTest, HasOutputDevices) {
  EXPECT_CALL(*this, HasOutputDevicesCallback(true));
  audio_system_->HasOutputDevices(base::Bind(
      &AudioSystemImplTest::OnHasOutputDevices, base::Unretained(this)));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, HasNoOutputDevices) {
  audio_manager_->SetHasOutputDevices(false);
  EXPECT_CALL(*this, HasOutputDevicesCallback(false));
  audio_system_->HasOutputDevices(base::Bind(
      &AudioSystemImplTest::OnHasOutputDevices, base::Unretained(this)));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetInputDeviceDescriptionsNoInputDevices) {
  output_device_descriptions_.emplace_back("output_device_name",
                                           "output_device_id", "group_id");
  EXPECT_EQ(0, static_cast<int>(input_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(output_device_descriptions_.size()));
  EXPECT_CALL(*this, DeviceDescriptionsReceived());
  audio_system_->GetDeviceDescriptions(
      true, base::Bind(&AudioSystemImplTest::OnGetDeviceDescriptions,
                       base::Unretained(this), input_device_descriptions_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetInputDeviceDescriptions) {
  output_device_descriptions_.emplace_back("output_device_name",
                                           "output_device_id", "group_id");
  input_device_descriptions_.emplace_back("input_device_name1",
                                          "input_device_id1", "group_id1");
  input_device_descriptions_.emplace_back("input_device_name2",
                                          "input_device_id2", "group_id2");
  EXPECT_EQ(2, static_cast<int>(input_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(output_device_descriptions_.size()));
  EXPECT_CALL(*this, DeviceDescriptionsReceived());
  audio_system_->GetDeviceDescriptions(
      true, base::Bind(&AudioSystemImplTest::OnGetDeviceDescriptions,
                       base::Unretained(this), input_device_descriptions_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetOutputDeviceDescriptionsNoInputDevices) {
  input_device_descriptions_.emplace_back("input_device_name",
                                          "input_device_id", "group_id");
  EXPECT_EQ(0, static_cast<int>(output_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(input_device_descriptions_.size()));
  EXPECT_CALL(*this, DeviceDescriptionsReceived());
  audio_system_->GetDeviceDescriptions(
      false, base::Bind(&AudioSystemImplTest::OnGetDeviceDescriptions,
                        base::Unretained(this), output_device_descriptions_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetOutputDeviceDescriptions) {
  input_device_descriptions_.emplace_back("input_device_name",
                                          "input_device_id", "group_id");
  output_device_descriptions_.emplace_back("output_device_name1",
                                           "output_device_id1", "group_id1");
  output_device_descriptions_.emplace_back("output_device_name2",
                                           "output_device_id2", "group_id2");
  EXPECT_EQ(2, static_cast<int>(output_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(input_device_descriptions_.size()));
  EXPECT_CALL(*this, DeviceDescriptionsReceived());
  audio_system_->GetDeviceDescriptions(
      false, base::Bind(&AudioSystemImplTest::OnGetDeviceDescriptions,
                        base::Unretained(this), output_device_descriptions_));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetAssociatedOutputDeviceID) {
  const std::string associated_id("associated_id");
  audio_manager_->SetAssociatedOutputDeviceIDCallback(
      base::Bind([](const std::string& result,
                    const std::string&) -> std::string { return result; },
                 associated_id));

  EXPECT_CALL(*this, AssociatedOutputDeviceIDReceived(associated_id));

  audio_system_->GetAssociatedOutputDeviceID(
      std::string(),
      base::Bind(&AudioSystemImplTest::AssociatedOutputDeviceIDReceived,
                 base::Unretained(this)));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetInputDeviceInfoNoAssociation) {
  EXPECT_CALL(*this, InputDeviceInfoReceived());

  audio_system_->GetInputDeviceInfo(
      kNonDefaultDeviceId,
      base::Bind(&AudioSystemImplTest::OnInputDeviceInfo,
                 base::Unretained(this), input_params_,
                 base::Optional<AudioParameters>(), std::string()));
  WaitForCallback();
}

TEST_P(AudioSystemImplTest, GetInputDeviceInfoWithAssociation) {
  EXPECT_CALL(*this, InputDeviceInfoReceived());

  const std::string associated_id("associated_id");
  audio_manager_->SetAssociatedOutputDeviceIDCallback(
      base::Bind([](const std::string& result,
                    const std::string&) -> std::string { return result; },
                 associated_id));

  audio_system_->GetInputDeviceInfo(
      kNonDefaultDeviceId, base::Bind(&AudioSystemImplTest::OnInputDeviceInfo,
                                      base::Unretained(this), input_params_,
                                      output_params_, associated_id));
  WaitForCallback();
}

INSTANTIATE_TEST_CASE_P(, AudioSystemImplTest, testing::Values(false, true));

}  // namespace media
