// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The format of these tests are to enqueue a known amount of data and then
// request the exact amount we expect in order to dequeue the known amount of
// data.  This ensures that for any rate we are consuming input data at the
// correct rate.  We always pass in a very large destination buffer with the
// expectation that FillBuffer() will fill as much as it can but no more.

#include "base/bind.h"
#include "base/callback.h"
#include "media/base/data_buffer.h"
#include "media/filters/audio_renderer_algorithm_ola.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;

namespace media {

class MockDataProvider {
 public:
  MockDataProvider() {}

  MOCK_METHOD0(Read, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDataProvider);
};

static const int kChannels = 1;
static const int kSampleRate = 1000;
static const int kSampleBits = 8;

TEST(AudioRendererAlgorithmOLATest, FillBuffer_NormalRate) {
  // When playback rate == 1.0f: straight copy of whatever is in |queue_|.
  MockDataProvider mock;
  AudioRendererAlgorithmOLA algorithm;
  algorithm.Initialize(kChannels, kSampleRate, kSampleBits, 1.0f,
                       base::Bind(&MockDataProvider::Read,
                                  base::Unretained(&mock)));

  // We won't reply to any read requests.
  EXPECT_CALL(mock, Read()).Times(AnyNumber());

  // Enqueue a buffer of any size since it doesn't matter.
  const size_t kDataSize = 1024;
  algorithm.EnqueueBuffer(new DataBuffer(new uint8[kDataSize], kDataSize));
  EXPECT_EQ(kDataSize, algorithm.QueueSize());

  // Read the same sized amount.
  scoped_array<uint8> data(new uint8[kDataSize]);
  EXPECT_EQ(kDataSize, algorithm.FillBuffer(data.get(), kDataSize));
  EXPECT_EQ(0u, algorithm.QueueSize());
}

TEST(AudioRendererAlgorithmOLATest, FillBuffer_DoubleRate) {
  // When playback rate > 1.0f: input is read faster than output is written.
  MockDataProvider mock;
  AudioRendererAlgorithmOLA algorithm;
  algorithm.Initialize(kChannels, kSampleRate, kSampleBits, 2.0f,
                       base::Bind(&MockDataProvider::Read,
                                  base::Unretained(&mock)));

  // We won't reply to any read requests.
  EXPECT_CALL(mock, Read()).Times(AnyNumber());

  // First parameter is the input buffer size, second parameter is how much data
  // we expect to consume in order to have no data left in the |algorithm|.
  //
  // For rate == 0.5f, reading half the input size should consume all enqueued
  // data.
  const size_t kBufferSize = 16 * 1024;
  scoped_array<uint8> data(new uint8[kBufferSize]);
  const size_t kTestData[][2] = {
    { algorithm.window_size_, algorithm.window_size_ / 2},
    { algorithm.window_size_ / 2, algorithm.window_size_ / 4},
    { 4u, 2u },
    { 0u, 0u },
  };

  for (size_t i = 0u; i < arraysize(kTestData); ++i) {
    const size_t kDataSize = kTestData[i][0];
    algorithm.EnqueueBuffer(new DataBuffer(new uint8[kDataSize], kDataSize));
    EXPECT_EQ(kDataSize, algorithm.QueueSize());

    const size_t kExpectedSize = kTestData[i][1];
    ASSERT_LE(kExpectedSize, kBufferSize);
    EXPECT_EQ(kExpectedSize, algorithm.FillBuffer(data.get(), kBufferSize));
    EXPECT_EQ(0u, algorithm.QueueSize());
  }
}

TEST(AudioRendererAlgorithmOLATest, FillBuffer_HalfRate) {
  // When playback rate < 1.0f: input is read slower than output is written.
  MockDataProvider mock;
  AudioRendererAlgorithmOLA algorithm;
  algorithm.Initialize(kChannels, kSampleRate, kSampleBits, 0.5f,
                       base::Bind(&MockDataProvider::Read,
                                  base::Unretained(&mock)));

  // We won't reply to any read requests.
  EXPECT_CALL(mock, Read()).Times(AnyNumber());

  // First parameter is the input buffer size, second parameter is how much data
  // we expect to consume in order to have no data left in the |algorithm|.
  //
  // For rate == 0.5f, reading double the input size should consume all enqueued
  // data.
  const size_t kBufferSize = 16 * 1024;
  scoped_array<uint8> data(new uint8[kBufferSize]);
  const size_t kTestData[][2] = {
    { algorithm.window_size_, algorithm.window_size_ * 2 },
    { algorithm.window_size_ / 2, algorithm.window_size_ },
    { 2u, 4u },
    { 0u, 0u },
  };

  for (size_t i = 0u; i < arraysize(kTestData); ++i) {
    const size_t kDataSize = kTestData[i][0];
    algorithm.EnqueueBuffer(new DataBuffer(new uint8[kDataSize], kDataSize));
    EXPECT_EQ(kDataSize, algorithm.QueueSize());

    const size_t kExpectedSize = kTestData[i][1];
    ASSERT_LE(kExpectedSize, kBufferSize);
    EXPECT_EQ(kExpectedSize, algorithm.FillBuffer(data.get(), kBufferSize));
    EXPECT_EQ(0u, algorithm.QueueSize());
  }
}

TEST(AudioRendererAlgorithmOLATest, FillBuffer_QuarterRate) {
  // When playback rate is very low the audio is simply muted.
  MockDataProvider mock;
  AudioRendererAlgorithmOLA algorithm;
  algorithm.Initialize(kChannels, kSampleRate, kSampleBits, 0.25f,
                       base::Bind(&MockDataProvider::Read,
                                  base::Unretained(&mock)));

  // We won't reply to any read requests.
  EXPECT_CALL(mock, Read()).Times(AnyNumber());

  // First parameter is the input buffer size, second parameter is how much data
  // we expect to consume in order to have no data left in the |algorithm|.
  //
  // For rate == 0.25f, reading four times the input size should consume all
  // enqueued data but without executing OLA.
  const size_t kBufferSize = 16 * 1024;
  scoped_array<uint8> data(new uint8[kBufferSize]);
  const size_t kTestData[][2] = {
    { algorithm.window_size_, algorithm.window_size_ * 4},
    { algorithm.window_size_ / 2, algorithm.window_size_ * 2},
    { 1u, 4u },
    { 0u, 0u },
  };

  for (size_t i = 0u; i < arraysize(kTestData); ++i) {
    const size_t kDataSize = kTestData[i][0];
    algorithm.EnqueueBuffer(new DataBuffer(new uint8[kDataSize], kDataSize));
    EXPECT_EQ(kDataSize, algorithm.QueueSize());

    const size_t kExpectedSize = kTestData[i][1];
    ASSERT_LE(kExpectedSize, kBufferSize);
    EXPECT_EQ(kExpectedSize, algorithm.FillBuffer(data.get(), kBufferSize));
    EXPECT_EQ(0u, algorithm.QueueSize());
  }
}

}  // namespace media
