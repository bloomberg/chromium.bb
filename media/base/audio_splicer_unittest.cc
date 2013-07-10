// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_splicer.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const SampleFormat kSampleFormat = kSampleFormatF32;
static const int kChannels = 1;
static const int kDefaultSampleRate = 44100;
static const int kDefaultBufferSize = 100;

class AudioSplicerTest : public ::testing::Test {
 public:
  AudioSplicerTest()
      : splicer_(kDefaultSampleRate),
        input_timestamp_helper_(kDefaultSampleRate) {
    input_timestamp_helper_.SetBaseTimestamp(base::TimeDelta());
  }

  scoped_refptr<AudioBuffer> GetNextInputBuffer(float value) {
    return GetNextInputBuffer(value, kDefaultBufferSize);
  }

  scoped_refptr<AudioBuffer> GetNextInputBuffer(float value, int frame_size) {
    scoped_refptr<AudioBuffer> buffer = MakeInterleavedAudioBuffer<float>(
        kSampleFormat,
        kChannels,
        value,
        0.0f,
        frame_size,
        input_timestamp_helper_.GetTimestamp(),
        input_timestamp_helper_.GetFrameDuration(frame_size));
    input_timestamp_helper_.AddFrames(frame_size);
    return buffer;
  }

  bool VerifyData(scoped_refptr<AudioBuffer> buffer, float value) {
    int frames = buffer->frame_count();
    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, frames);
    buffer->ReadFrames(frames, 0, 0, bus.get());
    for (int i = 0; i < frames; ++i) {
      if (bus->channel(0)[i] != value)
        return false;
    }
    return true;
  }

 protected:
  AudioSplicer splicer_;
  AudioTimestampHelper input_timestamp_helper_;

  DISALLOW_COPY_AND_ASSIGN(AudioSplicerTest);
};

TEST_F(AudioSplicerTest, PassThru) {
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Test single buffer pass-thru behavior.
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);
  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<AudioBuffer> output_1 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_1->timestamp(), output_1->timestamp());
  EXPECT_EQ(input_1->duration(), output_1->duration());
  EXPECT_EQ(input_1->frame_count(), output_1->frame_count());
  EXPECT_TRUE(VerifyData(output_1, 0.1f));

  // Test that multiple buffers can be queued in the splicer.
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.3f);
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.AddInput(input_3));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_TRUE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_2->timestamp(), output_2->timestamp());
  EXPECT_EQ(input_2->duration(), output_2->duration());
  EXPECT_EQ(input_2->frame_count(), output_2->frame_count());

  scoped_refptr<AudioBuffer> output_3 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_3->timestamp(), output_3->timestamp());
  EXPECT_EQ(input_3->duration(), output_3->duration());
  EXPECT_EQ(input_3->frame_count(), output_3->frame_count());
}

TEST_F(AudioSplicerTest, Reset) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);
  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  splicer_.Reset();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Add some bytes to the timestamp helper so that the
  // next buffer starts many frames beyond the end of
  // |input_1|. This is to make sure that Reset() actually
  // clears its state and doesn't try to insert a gap.
  input_timestamp_helper_.AddFrames(100);

  // Verify that a new input buffer passes through as expected.
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_2->timestamp(), output_2->timestamp());
  EXPECT_EQ(input_2->duration(), output_2->duration());
  EXPECT_EQ(input_2->frame_count(), output_2->frame_count());
}

TEST_F(AudioSplicerTest, EndOfStream) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);
  scoped_refptr<AudioBuffer> input_2 = AudioBuffer::CreateEOSBuffer();
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.2f);
  EXPECT_TRUE(input_2->end_of_stream());

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.HasNextBuffer());

  scoped_refptr<AudioBuffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_1->timestamp(), output_1->timestamp());
  EXPECT_EQ(input_1->duration(), output_1->duration());
  EXPECT_EQ(input_1->frame_count(), output_1->frame_count());

  EXPECT_TRUE(output_2->end_of_stream());

  // Verify that buffers can be added again after Reset().
  splicer_.Reset();
  EXPECT_TRUE(splicer_.AddInput(input_3));
  scoped_refptr<AudioBuffer> output_3 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_3->timestamp(), output_3->timestamp());
  EXPECT_EQ(input_3->duration(), output_3->duration());
  EXPECT_EQ(input_3->frame_count(), output_3->frame_count());
}


// Test the gap insertion code.
// +--------------+    +--------------+
// |11111111111111|    |22222222222222|
// +--------------+    +--------------+
// Results in:
// +--------------+----+--------------+
// |11111111111111|0000|22222222222222|
// +--------------+----+--------------+
TEST_F(AudioSplicerTest, GapInsertion) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Add bytes to the timestamp helper so that the next buffer
  // will have a starting timestamp that indicates a gap is
  // present.
  const int kGapSize = 7;
  input_timestamp_helper_.AddFrames(kGapSize);
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));

  // Verify that a gap buffer is generated.
  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  scoped_refptr<AudioBuffer> output_3 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->timestamp(), output_1->timestamp());
  EXPECT_EQ(input_1->duration(), output_1->duration());
  EXPECT_EQ(input_1->frame_count(), output_1->frame_count());
  EXPECT_TRUE(VerifyData(output_1, 0.1f));

  // Verify the contents of the gap buffer.
  base::TimeDelta gap_timestamp =
      input_1->timestamp() + input_1->duration();
  base::TimeDelta gap_duration = input_2->timestamp() - gap_timestamp;
  EXPECT_GT(gap_duration, base::TimeDelta());
  EXPECT_EQ(gap_timestamp, output_2->timestamp());
  EXPECT_EQ(gap_duration, output_2->duration());
  EXPECT_EQ(kGapSize, output_2->frame_count());
  EXPECT_TRUE(VerifyData(output_2, 0.0f));

  // Verify that the second input buffer passed through unmodified.
  EXPECT_EQ(input_2->timestamp(), output_3->timestamp());
  EXPECT_EQ(input_2->duration(), output_3->duration());
  EXPECT_EQ(input_2->frame_count(), output_3->frame_count());
  EXPECT_TRUE(VerifyData(output_3, 0.2f));
}


// Test that an error is signalled when the gap between input buffers is
// too large.
TEST_F(AudioSplicerTest, GapTooLarge) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Add a seconds worth of bytes so that an unacceptably large
  // gap exists between |input_1| and |input_2|.
  const int kGapSize = kDefaultSampleRate;
  input_timestamp_helper_.AddFrames(kGapSize);
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_FALSE(splicer_.AddInput(input_2));

  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> output_1 = splicer_.GetNextBuffer();

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->timestamp(), output_1->timestamp());
  EXPECT_EQ(input_1->duration(), output_1->duration());
  EXPECT_EQ(input_1->frame_count(), output_1->frame_count());
  EXPECT_TRUE(VerifyData(output_1, 0.1f));

  // Verify that the second buffer is not available.
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Reset the timestamp helper so it can generate a buffer that is
  // right after |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(
      input_1->timestamp() + input_1->duration());

  // Verify that valid buffers are still accepted.
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.3f);
  EXPECT_TRUE(splicer_.AddInput(input_3));
  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());
  EXPECT_EQ(input_3->timestamp(), output_2->timestamp());
  EXPECT_EQ(input_3->duration(), output_2->duration());
  EXPECT_EQ(input_3->frame_count(), output_2->frame_count());
  EXPECT_TRUE(VerifyData(output_2, 0.3f));
}


// Verifies that an error is signalled if AddInput() is called
// with a timestamp that is earlier than the first buffer added.
TEST_F(AudioSplicerTest, BufferAddedBeforeBase) {
  input_timestamp_helper_.SetBaseTimestamp(
      base::TimeDelta::FromMicroseconds(10));
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Reset the timestamp helper so the next buffer will have a timestamp earlier
  // than |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(base::TimeDelta::FromSeconds(0));
  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.1f);

  EXPECT_GT(input_1->timestamp(), input_2->timestamp());
  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_FALSE(splicer_.AddInput(input_2));
}


// Test when one buffer partially overlaps another.
// +--------------+
// |11111111111111|
// +--------------+
//            +--------------+
//            |22222222222222|
//            +--------------+
// Results in:
// +--------------+----------+
// |11111111111111|2222222222|
// +--------------+----------+
TEST_F(AudioSplicerTest, PartialOverlap) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Reset timestamp helper so that the next buffer will have a
  // timestamp that starts in the middle of |input_1|.
  const int kOverlapSize = input_1->frame_count() / 4;
  input_timestamp_helper_.SetBaseTimestamp(input_1->timestamp());
  input_timestamp_helper_.AddFrames(input_1->frame_count() - kOverlapSize);

  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));

  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->timestamp(), output_1->timestamp());
  EXPECT_EQ(input_1->duration(), output_1->duration());
  EXPECT_EQ(input_1->frame_count(), output_1->frame_count());
  EXPECT_TRUE(VerifyData(output_1, 0.1f));

  // Verify that the second input buffer was truncated to only contain
  // the samples that are after the end of |input_1|. Note that data is not
  // copied, so |input_2|'s values are modified.
  base::TimeDelta expected_timestamp =
      input_1->timestamp() + input_1->duration();
  base::TimeDelta expected_duration =
      (input_2->timestamp() + input_2->duration()) - expected_timestamp;
  EXPECT_EQ(expected_timestamp, output_2->timestamp());
  EXPECT_EQ(expected_duration, output_2->duration());
  EXPECT_TRUE(VerifyData(output_2, 0.2f));
}


// Test that an input buffer that is completely overlapped by a buffer
// that was already added is dropped.
// +--------------+
// |11111111111111|
// +--------------+
//       +-----+
//       |22222|
//       +-----+
//                +-------------+
//                |3333333333333|
//                +-------------+
// Results in:
// +--------------+-------------+
// |11111111111111|3333333333333|
// +--------------+-------------+
TEST_F(AudioSplicerTest, DropBuffer) {
  scoped_refptr<AudioBuffer> input_1 = GetNextInputBuffer(0.1f);

  // Reset timestamp helper so that the next buffer will have a
  // timestamp that starts in the middle of |input_1|.
  const int kOverlapOffset = input_1->frame_count() / 2;
  const int kOverlapSize = input_1->frame_count() / 4;
  input_timestamp_helper_.SetBaseTimestamp(input_1->timestamp());
  input_timestamp_helper_.AddFrames(kOverlapOffset);

  scoped_refptr<AudioBuffer> input_2 = GetNextInputBuffer(0.2f, kOverlapSize);

  // Reset the timestamp helper so the next buffer will be right after
  // |input_1|.
  input_timestamp_helper_.SetBaseTimestamp(input_1->timestamp());
  input_timestamp_helper_.AddFrames(input_1->frame_count());
  scoped_refptr<AudioBuffer> input_3 = GetNextInputBuffer(0.3f);

  EXPECT_TRUE(splicer_.AddInput(input_1));
  EXPECT_TRUE(splicer_.AddInput(input_2));
  EXPECT_TRUE(splicer_.AddInput(input_3));

  EXPECT_TRUE(splicer_.HasNextBuffer());
  scoped_refptr<AudioBuffer> output_1 = splicer_.GetNextBuffer();
  scoped_refptr<AudioBuffer> output_2 = splicer_.GetNextBuffer();
  EXPECT_FALSE(splicer_.HasNextBuffer());

  // Verify that the first input buffer passed through unmodified.
  EXPECT_EQ(input_1->timestamp(), output_1->timestamp());
  EXPECT_EQ(input_1->duration(), output_1->duration());
  EXPECT_EQ(input_1->frame_count(), output_1->frame_count());
  EXPECT_TRUE(VerifyData(output_1, 0.1f));

  // Verify that the second output buffer only contains
  // the samples that are in |input_3|.
  EXPECT_EQ(input_3->timestamp(), output_2->timestamp());
  EXPECT_EQ(input_3->duration(), output_2->duration());
  EXPECT_EQ(input_3->frame_count(), output_2->frame_count());
  EXPECT_TRUE(VerifyData(output_2, 0.3f));
}

}  // namespace media
