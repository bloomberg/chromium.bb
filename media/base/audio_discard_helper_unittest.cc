// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const float kDataStep = 0.01f;
static const size_t kSampleRate = 48000;

static scoped_refptr<DecoderBuffer> CreateEncodedBuffer(
    base::TimeDelta timestamp,
    base::TimeDelta duration) {
  scoped_refptr<DecoderBuffer> result(new DecoderBuffer(1));
  result->set_timestamp(timestamp);
  result->set_duration(duration);
  return result;
}

static scoped_refptr<AudioBuffer> CreateDecodedBuffer(int frames) {
  return MakeAudioBuffer(kSampleFormatPlanarF32,
                         CHANNEL_LAYOUT_MONO,
                         1,
                         kSampleRate,
                         0.0f,
                         kDataStep,
                         frames,
                         kNoTimestamp(),
                         kNoTimestamp());
}

static float ExtractDecodedData(const scoped_refptr<AudioBuffer>& buffer,
                                int index) {
  // This is really inefficient, but we can't access the raw AudioBuffer if any
  // start trimming has been applied.
  scoped_ptr<AudioBus> temp_bus = AudioBus::Create(buffer->channel_count(), 1);
  buffer->ReadFrames(1, index, 0, temp_bus.get());
  return temp_bus->channel(0)[0];
}

TEST(AudioDiscardHelperTest, TimeDeltaToFrames) {
  AudioDiscardHelper discard_helper(kSampleRate);

  EXPECT_EQ(0u, discard_helper.TimeDeltaToFrames(base::TimeDelta()));
  EXPECT_EQ(
      kSampleRate / 100,
      discard_helper.TimeDeltaToFrames(base::TimeDelta::FromMilliseconds(10)));

  // Ensure partial frames are rounded down correctly.  The equation below
  // calculates a frame count with a fractional part < 0.5.
  const int small_remainder =
      base::Time::kMicrosecondsPerSecond * (kSampleRate - 0.9) / kSampleRate;
  EXPECT_EQ(kSampleRate - 1,
            discard_helper.TimeDeltaToFrames(
                base::TimeDelta::FromMicroseconds(small_remainder)));

  // Ditto, but rounded up using a fractional part > 0.5.
  const int large_remainder =
      base::Time::kMicrosecondsPerSecond * (kSampleRate - 0.4) / kSampleRate;
  EXPECT_EQ(kSampleRate,
            discard_helper.TimeDeltaToFrames(
                base::TimeDelta::FromMicroseconds(large_remainder)));
}

TEST(AudioDiscardHelperTest, BasicProcessBuffers) {
  AudioDiscardHelper discard_helper(kSampleRate);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();

  // Use an estimated duration which doesn't match the number of decoded frames
  // to ensure the helper is correctly setting durations based on output frames.
  const base::TimeDelta kEstimatedDuration =
      base::TimeDelta::FromMilliseconds(9);
  const base::TimeDelta kActualDuration = base::TimeDelta::FromMilliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kActualDuration);

  scoped_refptr<DecoderBuffer> encoded_buffer =
      CreateEncodedBuffer(kTimestamp, kEstimatedDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify the basic case where nothing is discarded.
  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kActualDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());

  // Verify a Reset() takes us back to an uninitialized state.
  discard_helper.Reset(0);
  ASSERT_FALSE(discard_helper.initialized());

  // Verify a NULL output buffer returns false.
  ASSERT_FALSE(discard_helper.ProcessBuffers(encoded_buffer, NULL));
}

TEST(AudioDiscardHelperTest, NegativeTimestampClampsToZero) {
  AudioDiscardHelper discard_helper(kSampleRate);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = -base::TimeDelta::FromSeconds(1);
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  scoped_refptr<DecoderBuffer> encoded_buffer =
      CreateEncodedBuffer(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify the basic case where nothing is discarded.
  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(base::TimeDelta(), decoded_buffer->timestamp());
  EXPECT_EQ(kDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());
}

TEST(AudioDiscardHelperTest, ProcessBuffersWithInitialDiscard) {
  AudioDiscardHelper discard_helper(kSampleRate);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  // Tell the helper we want to discard half of the initial frames.
  const int kDiscardFrames = kTestFrames / 2;
  discard_helper.Reset(kDiscardFrames);

  scoped_refptr<DecoderBuffer> encoded_buffer =
      CreateEncodedBuffer(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Verify half the frames end up discarded.
  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kDiscardFrames, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kDiscardFrames * kDataStep,
                  ExtractDecodedData(decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, ProcessBuffersWithLargeInitialDiscard) {
  AudioDiscardHelper discard_helper(kSampleRate);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  // Tell the helper we want to discard 1.5 buffers worth of frames.
  discard_helper.Reset(kTestFrames * 1.5);

  scoped_refptr<DecoderBuffer> encoded_buffer =
      CreateEncodedBuffer(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // The first call should fail since no output buffer remains.
  ASSERT_FALSE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  ASSERT_TRUE(discard_helper.initialized());

  // Generate another set of buffers and expect half the output frames.
  encoded_buffer = CreateEncodedBuffer(kTimestamp + kDuration, kDuration);
  decoded_buffer = CreateDecodedBuffer(kTestFrames);
  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));

  // The timestamp should match that of the initial buffer.
  const int kDiscardFrames = kTestFrames / 2;
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kDiscardFrames, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kDiscardFrames * kDataStep,
                  ExtractDecodedData(decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, AllowNonMonotonicTimestamps) {
  AudioDiscardHelper discard_helper(kSampleRate);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  scoped_refptr<DecoderBuffer> encoded_buffer =
      CreateEncodedBuffer(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());

  // Process the same input buffer again to ensure input timestamps which go
  // backwards in time are not errors.
  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  EXPECT_EQ(kTimestamp + kDuration, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames, decoded_buffer->frame_count());
}

TEST(AudioDiscardHelperTest, DiscardPadding) {
  AudioDiscardHelper discard_helper(kSampleRate);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  scoped_refptr<DecoderBuffer> encoded_buffer =
      CreateEncodedBuffer(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Set a discard padding equivalent to half the buffer.
  encoded_buffer->set_discard_padding(kDuration / 2);

  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(0, ExtractDecodedData(decoded_buffer, 0));
}

TEST(AudioDiscardHelperTest, InitialDiscardAndDiscardPadding) {
  AudioDiscardHelper discard_helper(kSampleRate);
  ASSERT_FALSE(discard_helper.initialized());

  const base::TimeDelta kTimestamp = base::TimeDelta();
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(10);
  const int kTestFrames = discard_helper.TimeDeltaToFrames(kDuration);

  scoped_refptr<DecoderBuffer> encoded_buffer =
      CreateEncodedBuffer(kTimestamp, kDuration);
  scoped_refptr<AudioBuffer> decoded_buffer = CreateDecodedBuffer(kTestFrames);

  // Set a discard padding equivalent to a quarter of the buffer.
  encoded_buffer->set_discard_padding(kDuration / 4);

  // Set an initial discard of a quarter of the buffer.
  const int kDiscardFrames = kTestFrames / 4;
  discard_helper.Reset(kDiscardFrames);

  ASSERT_TRUE(discard_helper.ProcessBuffers(encoded_buffer, decoded_buffer));
  ASSERT_TRUE(discard_helper.initialized());
  EXPECT_EQ(kTimestamp, decoded_buffer->timestamp());
  EXPECT_EQ(kDuration / 2, decoded_buffer->duration());
  EXPECT_EQ(kTestFrames / 2, decoded_buffer->frame_count());
  ASSERT_FLOAT_EQ(kDiscardFrames * kDataStep,
                  ExtractDecodedData(decoded_buffer, 0));
}

}  // namespace media
