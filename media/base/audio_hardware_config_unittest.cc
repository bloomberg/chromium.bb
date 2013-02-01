// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_hardware_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kOutputBufferSize = 2048;
static const int kOutputSampleRate = 48000;
static const int kInputSampleRate = 44100;
static const ChannelLayout kInputChannelLayout = CHANNEL_LAYOUT_STEREO;

TEST(AudioHardwareConfig, Getters) {
  AudioHardwareConfig fake_config(
      kOutputBufferSize, kOutputSampleRate, kInputSampleRate,
      kInputChannelLayout);

  EXPECT_EQ(kOutputBufferSize, fake_config.GetOutputBufferSize());
  EXPECT_EQ(kOutputSampleRate, fake_config.GetOutputSampleRate());
  EXPECT_EQ(kInputSampleRate, fake_config.GetInputSampleRate());
  EXPECT_EQ(kInputChannelLayout, fake_config.GetInputChannelLayout());
}

TEST(AudioHardwareConfig, Setters) {
  AudioHardwareConfig fake_config(
      kOutputBufferSize, kOutputSampleRate, kInputSampleRate,
      kInputChannelLayout);

  // Verify output parameters.
  const int kNewOutputBufferSize = kOutputBufferSize * 2;
  const int kNewOutputSampleRate = kOutputSampleRate * 2;
  EXPECT_NE(kNewOutputBufferSize, fake_config.GetOutputBufferSize());
  EXPECT_NE(kNewOutputSampleRate, fake_config.GetOutputSampleRate());
  fake_config.UpdateOutputConfig(kNewOutputBufferSize, kNewOutputSampleRate);
  EXPECT_EQ(kNewOutputBufferSize, fake_config.GetOutputBufferSize());
  EXPECT_EQ(kNewOutputSampleRate, fake_config.GetOutputSampleRate());

  // Verify input parameters.
  const int kNewInputSampleRate = kInputSampleRate * 2;
  const ChannelLayout kNewInputChannelLayout = CHANNEL_LAYOUT_MONO;
  EXPECT_NE(kNewInputSampleRate, fake_config.GetInputSampleRate());
  EXPECT_NE(kNewInputChannelLayout, fake_config.GetInputChannelLayout());
  fake_config.UpdateInputConfig(kNewInputSampleRate, kNewInputChannelLayout);
  EXPECT_EQ(kNewInputSampleRate, fake_config.GetInputSampleRate());
  EXPECT_EQ(kNewInputChannelLayout, fake_config.GetInputChannelLayout());
}

}  // namespace content
