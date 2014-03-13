// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace test {
namespace {

TEST(AudioTimestampTest, Small) {
  std::vector<int16> samples(480);
  for (int32 in_timestamp = 0; in_timestamp < 65536; in_timestamp += 177) {
    EncodeTimestamp(in_timestamp, 0, &samples);
    uint16 out_timestamp;
    EXPECT_TRUE(DecodeTimestamp(samples, &out_timestamp));
    ASSERT_EQ(in_timestamp, out_timestamp);
  }
}

TEST(AudioTimestampTest, Negative) {
  std::vector<int16> samples(480);
  uint16 out_timestamp;
  EXPECT_FALSE(DecodeTimestamp(samples, &out_timestamp));
}

TEST(AudioTimestampTest, CheckPhase) {
  std::vector<int16> samples(4800);
  EncodeTimestamp(4711, 0, &samples);
  while (samples.size() > 240) {
    uint16 out_timestamp;
    EXPECT_TRUE(DecodeTimestamp(samples, &out_timestamp));
    ASSERT_EQ(4711, out_timestamp);

    samples.erase(samples.begin(), samples.begin() + 73);
  }
}

}  // namespace
}  // namespace test
}  // namespace cast
}  // namespace media
