// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/base/mock_media_log.h"
#include "media/formats/mp4/aac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

namespace mp4 {

MATCHER_P(AudioCodecLog, codec_string, "") {
  return CONTAINS_STRING(arg, "Audio codec: " + std::string(codec_string));
}

class AACTest : public testing::Test {
 public:
  AACTest() : media_log_(new StrictMock<MockMediaLog>()) {}

  bool Parse(const std::vector<uint8>& data) {
    return aac_.Parse(data, media_log_);
  }

  scoped_refptr<StrictMock<MockMediaLog>> media_log_;
  AAC aac_;
};

TEST_F(AACTest, BasicProfileTest) {
  uint8 buffer[] = {0x12, 0x10};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.2"));

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 44100);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
}

TEST_F(AACTest, ExtensionTest) {
  uint8 buffer[] = {0x13, 0x08, 0x56, 0xe5, 0x9d, 0x48, 0x80};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.2"));

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 48000);
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
}

// Test implicit SBR with mono channel config.
// Mono channel layout should only be reported if SBR is not
// specified. Otherwise stereo should be reported.
// See ISO-14496-3 Section 1.6.6.1.2 for details about this special casing.
TEST_F(AACTest, ImplicitSBR_ChannelConfig0) {
  uint8 buffer[] = {0x13, 0x08};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.2"));

  EXPECT_TRUE(Parse(data));

  // Test w/o implict SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 24000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_MONO);

  // Test implicit SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(true), CHANNEL_LAYOUT_STEREO);
}

// Tests implicit SBR with a stereo channel config.
TEST_F(AACTest, ImplicitSBR_ChannelConfig1) {
  uint8 buffer[] = {0x13, 0x10};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.2"));

  EXPECT_TRUE(Parse(data));

  // Test w/o implict SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 24000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);

  // Test implicit SBR.
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(true), CHANNEL_LAYOUT_STEREO);
}

TEST_F(AACTest, SixChannelTest) {
  uint8 buffer[] = {0x11, 0xb0};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.2"));

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(aac_.GetOutputSamplesPerSecond(false), 48000);
  EXPECT_EQ(aac_.GetChannelLayout(false), CHANNEL_LAYOUT_5_1_BACK);
}

TEST_F(AACTest, DataTooShortTest) {
  std::vector<uint8> data;

  EXPECT_FALSE(Parse(data));

  data.push_back(0x12);
  EXPECT_FALSE(Parse(data));
}

TEST_F(AACTest, IncorrectProfileTest) {
  InSequence s;
  uint8 buffer[] = {0x0, 0x08};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.0"));
  EXPECT_FALSE(Parse(data));

  data[0] = 0x08;
  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.1"));
  EXPECT_TRUE(Parse(data));

  data[0] = 0x28;
  // No media log for this profile 5, since not enough bits are in |data| to
  // first parse profile 5's extension frequency index.
  EXPECT_FALSE(Parse(data));
}

TEST_F(AACTest, IncorrectFrequencyTest) {
  uint8 buffer[] = {0x0f, 0x88};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_FALSE(Parse(data));

  data[0] = 0x0e;
  data[1] = 0x08;
  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.1"));
  EXPECT_TRUE(Parse(data));
}

TEST_F(AACTest, IncorrectChannelTest) {
  uint8 buffer[] = {0x0e, 0x00};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_MEDIA_LOG(AudioCodecLog("mp4a.40.1")).Times(2);

  EXPECT_FALSE(Parse(data));

  data[1] = 0x08;
  EXPECT_TRUE(Parse(data));
}

}  // namespace mp4

}  // namespace media
