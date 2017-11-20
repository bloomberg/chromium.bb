// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_test_util.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"

namespace media {

bool operator==(const AudioDeviceDescription& lhs,
                const AudioDeviceDescription& rhs) {
  return lhs.device_name == rhs.device_name && lhs.unique_id == rhs.unique_id &&
         lhs.group_id == rhs.group_id;
}

AudioSystem::OnAudioParamsCallback
AudioSystemCallbackExpectations::GetAudioParamsCallback(
    const base::Location& location,
    base::OnceClosure on_cb_received,
    const base::Optional<AudioParameters>& expected_params) {
  return base::BindOnce(&AudioSystemCallbackExpectations::OnAudioParams,
                        base::Unretained(this), location.ToString(),
                        std::move(on_cb_received), expected_params);
}

AudioSystem::OnBoolCallback AudioSystemCallbackExpectations::GetBoolCallback(
    const base::Location& location,
    base::OnceClosure on_cb_received,
    bool expected) {
  return base::BindOnce(&AudioSystemCallbackExpectations::OnBool,
                        base::Unretained(this), location.ToString(),
                        std::move(on_cb_received), expected);
}

AudioSystem::OnDeviceDescriptionsCallback
AudioSystemCallbackExpectations::GetDeviceDescriptionsCallback(
    const base::Location& location,
    base::OnceClosure on_cb_received,
    const AudioDeviceDescriptions& expected_descriptions) {
  return base::BindOnce(&AudioSystemCallbackExpectations::OnDeviceDescriptions,
                        base::Unretained(this), location.ToString(),
                        std::move(on_cb_received), expected_descriptions);
}

AudioSystem::OnInputDeviceInfoCallback
AudioSystemCallbackExpectations::GetInputDeviceInfoCallback(
    const base::Location& location,
    base::OnceClosure on_cb_received,
    const base::Optional<AudioParameters>& expected_input,
    const base::Optional<AudioParameters>& expected_associated_output,
    const std::string& expected_associated_device_id) {
  return base::BindOnce(&AudioSystemCallbackExpectations::OnInputDeviceInfo,
                        base::Unretained(this), location.ToString(),
                        std::move(on_cb_received), expected_input,
                        expected_associated_output,
                        expected_associated_device_id);
}

AudioSystem::OnDeviceIdCallback
AudioSystemCallbackExpectations::GetDeviceIdCallback(
    const base::Location& location,
    base::OnceClosure on_cb_received,
    const std::string& expected_id) {
  return base::BindOnce(&AudioSystemCallbackExpectations::OnDeviceId,
                        base::Unretained(this), location.ToString(),
                        std::move(on_cb_received), expected_id);
}

void AudioSystemCallbackExpectations::OnAudioParams(
    const std::string& from_here,
    base::OnceClosure on_cb_received,
    const base::Optional<AudioParameters>& expected,
    const base::Optional<AudioParameters>& received) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) << from_here;
  if (expected) {
    EXPECT_TRUE(received) << from_here;
    EXPECT_EQ(expected->AsHumanReadableString(),
              received->AsHumanReadableString())
        << from_here;
  } else {
    EXPECT_FALSE(received) << from_here;
  }
  std::move(on_cb_received).Run();
}

void AudioSystemCallbackExpectations::OnBool(const std::string& from_here,
                                             base::OnceClosure on_cb_received,
                                             bool expected,
                                             bool result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) << from_here;
  EXPECT_EQ(expected, result) << from_here;
  std::move(on_cb_received).Run();
}

void AudioSystemCallbackExpectations::OnDeviceDescriptions(
    const std::string& from_here,
    base::OnceClosure on_cb_received,
    const AudioDeviceDescriptions& expected_descriptions,
    AudioDeviceDescriptions descriptions) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) << from_here;
  EXPECT_EQ(expected_descriptions, descriptions);
  std::move(on_cb_received).Run();
}

void AudioSystemCallbackExpectations::OnInputDeviceInfo(
    const std::string& from_here,
    base::OnceClosure on_cb_received,
    const base::Optional<AudioParameters>& expected_input,
    const base::Optional<AudioParameters>& expected_associated_output,
    const std::string& expected_associated_device_id,
    const base::Optional<AudioParameters>& input,
    const base::Optional<AudioParameters>& associated_output,
    const std::string& associated_device_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) << from_here;
  if (expected_input) {
    EXPECT_TRUE(input) << from_here;
    EXPECT_EQ(expected_input->AsHumanReadableString(),
              input->AsHumanReadableString())
        << from_here;
  } else {
    EXPECT_FALSE(input) << from_here;
  }
  if (expected_associated_output) {
    EXPECT_TRUE(associated_output) << from_here;
    EXPECT_EQ(expected_associated_output->AsHumanReadableString(),
              associated_output->AsHumanReadableString())
        << from_here;
  } else {
    EXPECT_FALSE(associated_output) << from_here;
  }
  EXPECT_EQ(expected_associated_device_id, associated_device_id) << from_here;
  std::move(on_cb_received).Run();
}

void AudioSystemCallbackExpectations::OnDeviceId(
    const std::string& from_here,
    base::OnceClosure on_cb_received,
    const std::string& expected_id,
    const std::string& result_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) << from_here;
  EXPECT_EQ(expected_id, result_id) << from_here;
  std::move(on_cb_received).Run();
}

}  // namespace media
