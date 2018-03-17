// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_software_wrapper.h"

#include "base/test/scoped_task_environment.h"
#include "chromecast/media/cma/test/mock_cma_backend.h"
#include "chromecast/public/media/decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Field;
using ::testing::Return;
using ::testing::_;

namespace chromecast {
namespace media {

class AudioDecoderSoftwareWrapperTest : public ::testing::Test {
 public:
  AudioDecoderSoftwareWrapperTest()
      : audio_decoder_software_wrapper_(&audio_decoder_) {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  MockCmaBackend::AudioDecoder audio_decoder_;
  AudioDecoderSoftwareWrapper audio_decoder_software_wrapper_;
};

TEST_F(AudioDecoderSoftwareWrapperTest, IsUsingSoftwareDecoder) {
  AudioConfig audio_config;
  audio_config.sample_format = kSampleFormatS16;
  audio_config.bytes_per_channel = 2;
  audio_config.channel_number = 2;
  audio_config.samples_per_second = 48000;

  EXPECT_CALL(audio_decoder_, SetConfig(Field(&AudioConfig::codec, kCodecPCM)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(audio_decoder_, SetConfig(Field(&AudioConfig::codec, kCodecOpus)))
      .WillRepeatedly(Return(false));

  audio_config.codec = kCodecPCM;
  EXPECT_TRUE(audio_decoder_software_wrapper_.SetConfig(audio_config));
  EXPECT_FALSE(audio_decoder_software_wrapper_.IsUsingSoftwareDecoder());

  audio_config.codec = kCodecOpus;
  EXPECT_TRUE(audio_decoder_software_wrapper_.SetConfig(audio_config));
  EXPECT_TRUE(audio_decoder_software_wrapper_.IsUsingSoftwareDecoder());
}

}  // namespace media
}  // namespace chromecast
