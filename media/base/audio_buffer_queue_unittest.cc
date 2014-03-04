// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_queue.h"
#include "media/base/audio_bus.h"
#include "media/base/buffers.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static void VerifyResult(float* channel_data,
                         int frames,
                         float start,
                         float increment) {
  for (int i = 0; i < frames; ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "i=%d/%d start=%f, increment=%f", i, frames, start, increment));
    ASSERT_EQ(start, channel_data[i]);
    start += increment;
  }
}

TEST(AudioBufferQueueTest, AppendAndClear) {
  const int channels = 1;
  const int frames = 8;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;
  EXPECT_EQ(0, buffer.frames());
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 10, 1, frames, kNoTime, kNoTime));
  EXPECT_EQ(frames, buffer.frames());
  buffer.Clear();
  EXPECT_EQ(0, buffer.frames());
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 20, 1, frames, kNoTime, kNoTime));
  EXPECT_EQ(frames, buffer.frames());
}

TEST(AudioBufferQueueTest, MultipleAppend) {
  const int channels = 1;
  const int frames = 8;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Append 40 frames in 5 buffers.
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 10, 1, frames, kNoTime, kNoTime));
  EXPECT_EQ(8, buffer.frames());
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 10, 1, frames, kNoTime, kNoTime));
  EXPECT_EQ(16, buffer.frames());
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 10, 1, frames, kNoTime, kNoTime));
  EXPECT_EQ(24, buffer.frames());
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 10, 1, frames, kNoTime, kNoTime));
  EXPECT_EQ(32, buffer.frames());
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 10, 1, frames, kNoTime, kNoTime));
  EXPECT_EQ(40, buffer.frames());
}

TEST(AudioBufferQueueTest, IteratorCheck) {
  const int channels = 1;
  const int frames = 8;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);

  // Append 40 frames in 5 buffers. Intersperse ReadFrames() to make the
  // iterator is pointing to the correct position.
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 10.0f, 1.0f, frames, kNoTime, kNoTime));
  EXPECT_EQ(8, buffer.frames());

  EXPECT_EQ(4, buffer.ReadFrames(4, 0, bus.get()));
  EXPECT_EQ(4, buffer.frames());
  VerifyResult(bus->channel(0), 4, 10.0f, 1.0f);

  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 20.0f, 1.0f, frames, kNoTime, kNoTime));
  EXPECT_EQ(12, buffer.frames());
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 30.0f, 1.0f, frames, kNoTime, kNoTime));
  EXPECT_EQ(20, buffer.frames());

  buffer.SeekFrames(16);
  EXPECT_EQ(4, buffer.ReadFrames(4, 0, bus.get()));
  EXPECT_EQ(0, buffer.frames());
  VerifyResult(bus->channel(0), 4, 34.0f, 1.0f);

  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 40.0f, 1.0f, frames, kNoTime, kNoTime));
  EXPECT_EQ(8, buffer.frames());
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 50.0f, 1.0f, frames, kNoTime, kNoTime));
  EXPECT_EQ(16, buffer.frames());

  EXPECT_EQ(4, buffer.ReadFrames(4, 0, bus.get()));
  VerifyResult(bus->channel(0), 4, 40.0f, 1.0f);

  // Read off the end of the buffer.
  EXPECT_EQ(12, buffer.frames());
  buffer.SeekFrames(8);
  EXPECT_EQ(4, buffer.ReadFrames(100, 0, bus.get()));
  VerifyResult(bus->channel(0), 4, 54.0f, 1.0f);
}

TEST(AudioBufferQueueTest, Seek) {
  const int channels = 2;
  const int frames = 6;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 6 frames of data.
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 1.0f, 1.0f, frames, kNoTime, kNoTime));
  EXPECT_EQ(6, buffer.frames());

  // Seek past 2 frames.
  buffer.SeekFrames(2);
  EXPECT_EQ(4, buffer.frames());

  // Seek to end of data.
  buffer.SeekFrames(4);
  EXPECT_EQ(0, buffer.frames());

  // At end, seek now fails unless 0 specified.
  buffer.SeekFrames(0);
}

TEST(AudioBufferQueueTest, ReadF32) {
  const int channels = 2;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 76 frames of data.
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 1.0f, 1.0f, 6, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 13.0f, 1.0f, 10, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 33.0f, 1.0f, 60, kNoTime, kNoTime));
  EXPECT_EQ(76, buffer.frames());

  // Read 3 frames from the buffer. F32 is interleaved, so ch[0] should be
  // 1, 3, 5, and ch[1] should be 2, 4, 6.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  EXPECT_EQ(3, buffer.ReadFrames(3, 0, bus.get()));
  EXPECT_EQ(73, buffer.frames());
  VerifyResult(bus->channel(0), 3, 1.0f, 2.0f);
  VerifyResult(bus->channel(1), 3, 2.0f, 2.0f);

  // Now read 5 frames, which will span buffers. Append the data into AudioBus.
  EXPECT_EQ(5, buffer.ReadFrames(5, 3, bus.get()));
  EXPECT_EQ(68, buffer.frames());
  VerifyResult(bus->channel(0), 8, 1.0f, 2.0f);
  VerifyResult(bus->channel(1), 8, 2.0f, 2.0f);

  // Now skip into the third buffer.
  buffer.SeekFrames(20);
  EXPECT_EQ(48, buffer.frames());

  // Now read 2 frames, which are in the third buffer.
  EXPECT_EQ(2, buffer.ReadFrames(2, 0, bus.get()));
  VerifyResult(bus->channel(0), 2, 57.0f, 2.0f);
  VerifyResult(bus->channel(1), 2, 58.0f, 2.0f);
}

TEST(AudioBufferQueueTest, ReadU8) {
  const int channels = 4;
  const int frames = 4;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 4 frames of data.
  buffer.Append(MakeAudioBuffer<uint8>(
      kSampleFormatU8, channels, 128, 1, frames, kNoTime, kNoTime));

  // Read all 4 frames from the buffer. Data is interleaved, so ch[0] should be
  // 128, 132, 136, 140, other channels similar. However, values are converted
  // from [0, 255] to [-1.0, 1.0] with a bias of 128. Thus the first buffer
  // value should be 0.0, then 1/127, 2/127, etc.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  EXPECT_EQ(4, buffer.ReadFrames(4, 0, bus.get()));
  EXPECT_EQ(0, buffer.frames());
  VerifyResult(bus->channel(0), 4, 0.0f, 4.0f / 127.0f);
  VerifyResult(bus->channel(1), 4, 1.0f / 127.0f, 4.0f / 127.0f);
  VerifyResult(bus->channel(2), 4, 2.0f / 127.0f, 4.0f / 127.0f);
  VerifyResult(bus->channel(3), 4, 3.0f / 127.0f, 4.0f / 127.0f);
}

TEST(AudioBufferQueueTest, ReadS16) {
  const int channels = 2;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 24 frames of data.
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatS16, channels, 1, 1, 4, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatS16, channels, 9, 1, 20, kNoTime, kNoTime));
  EXPECT_EQ(24, buffer.frames());

  // Read 6 frames from the buffer. Data is interleaved, so ch[0] should be
  // 1, 3, 5, 7, 9, 11, and ch[1] should be 2, 4, 6, 8, 10, 12.
  // Data is converted to float from -1.0 to 1.0 based on int16 range.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  EXPECT_EQ(6, buffer.ReadFrames(6, 0, bus.get()));
  EXPECT_EQ(18, buffer.frames());
  VerifyResult(bus->channel(0), 6, 1.0f / kint16max, 2.0f / kint16max);
  VerifyResult(bus->channel(1), 6, 2.0f / kint16max, 2.0f / kint16max);
}

TEST(AudioBufferQueueTest, ReadS32) {
  const int channels = 2;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 24 frames of data.
  buffer.Append(MakeAudioBuffer<int32>(
      kSampleFormatS32, channels, 1, 1, 4, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<int32>(
      kSampleFormatS32, channels, 9, 1, 20, kNoTime, kNoTime));
  EXPECT_EQ(24, buffer.frames());

  // Read 6 frames from the buffer. Data is interleaved, so ch[0] should be
  // 1, 3, 5, 7, 100, 106, and ch[1] should be 2, 4, 6, 8, 103, 109.
  // Data is converted to float from -1.0 to 1.0 based on int32 range.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  EXPECT_EQ(6, buffer.ReadFrames(6, 0, bus.get()));
  EXPECT_EQ(18, buffer.frames());
  VerifyResult(bus->channel(0), 6, 1.0f / kint32max, 2.0f / kint32max);
  VerifyResult(bus->channel(1), 6, 2.0f / kint32max, 2.0f / kint32max);

  // Read the next 2 frames.
  EXPECT_EQ(2, buffer.ReadFrames(2, 0, bus.get()));
  EXPECT_EQ(16, buffer.frames());
  VerifyResult(bus->channel(0), 2, 13.0f / kint32max, 2.0f / kint32max);
  VerifyResult(bus->channel(1), 2, 14.0f / kint32max, 2.0f / kint32max);
}

TEST(AudioBufferQueueTest, ReadF32Planar) {
  const int channels = 2;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 14 frames of data.
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatPlanarF32, channels, 1.0f, 1.0f, 4, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatPlanarF32, channels, 50.0f, 1.0f, 10, kNoTime, kNoTime));
  EXPECT_EQ(14, buffer.frames());

  // Read 6 frames from the buffer. F32 is planar, so ch[0] should be
  // 1, 2, 3, 4, 50, 51, and ch[1] should be 5, 6, 7, 8, 60, 61.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  EXPECT_EQ(6, buffer.ReadFrames(6, 0, bus.get()));
  EXPECT_EQ(8, buffer.frames());
  VerifyResult(bus->channel(0), 4, 1.0f, 1.0f);
  VerifyResult(bus->channel(0) + 4, 2, 50.0f, 1.0f);
  VerifyResult(bus->channel(1), 4, 5.0f, 1.0f);
  VerifyResult(bus->channel(1) + 4, 2, 60.0f, 1.0f);
}

TEST(AudioBufferQueueTest, ReadS16Planar) {
  const int channels = 2;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 24 frames of data.
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatPlanarS16, channels, 1, 1, 4, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatPlanarS16, channels, 100, 5, 20, kNoTime, kNoTime));
  EXPECT_EQ(24, buffer.frames());

  // Read 6 frames from the buffer. Data is planar, so ch[0] should be
  // 1, 2, 3, 4, 100, 105, and ch[1] should be 5, 6, 7, 8, 200, 205.
  // Data is converted to float from -1.0 to 1.0 based on int16 range.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  EXPECT_EQ(6, buffer.ReadFrames(6, 0, bus.get()));
  EXPECT_EQ(18, buffer.frames());
  VerifyResult(bus->channel(0), 4, 1.0f / kint16max, 1.0f / kint16max);
  VerifyResult(bus->channel(0) + 4, 2, 100.0f / kint16max, 5.0f / kint16max);
  VerifyResult(bus->channel(1), 4, 5.0f / kint16max, 1.0f / kint16max);
  VerifyResult(bus->channel(1) + 4, 2, 200.0f / kint16max, 5.0f / kint16max);
}

TEST(AudioBufferQueueTest, ReadManyChannels) {
  const int channels = 16;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 76 frames of data.
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 0.0f, 1.0f, 6, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 6.0f * channels, 1.0f, 10, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<float>(kSampleFormatF32,
                                       channels,
                                       16.0f * channels,
                                       1.0f,
                                       60,
                                       kNoTime,
                                       kNoTime));
  EXPECT_EQ(76, buffer.frames());

  // Read 3 frames from the buffer. F32 is interleaved, so ch[0] should be
  // 1, 17, 33, and ch[1] should be 2, 18, 34. Just check a few channels.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  EXPECT_EQ(30, buffer.ReadFrames(30, 0, bus.get()));
  EXPECT_EQ(46, buffer.frames());
  for (int i = 0; i < channels; ++i) {
    VerifyResult(bus->channel(i), 30, static_cast<float>(i), 16.0f);
  }
}

TEST(AudioBufferQueueTest, Peek) {
  const int channels = 4;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;

  // Add 60 frames of data.
  buffer.Append(MakeAudioBuffer<float>(
      kSampleFormatF32, channels, 0.0f, 1.0f, 60, kNoTime, kNoTime));
  EXPECT_EQ(60, buffer.frames());

  // Peek at the first 30 frames.
  scoped_ptr<AudioBus> bus1 = AudioBus::Create(channels, 100);
  EXPECT_EQ(60, buffer.frames());
  EXPECT_EQ(60, buffer.PeekFrames(100, 0, 0, bus1.get()));
  EXPECT_EQ(30, buffer.PeekFrames(30, 0, 0, bus1.get()));
  EXPECT_EQ(60, buffer.frames());

  // Now read the next 30 frames (which should be the same as those peeked at).
  scoped_ptr<AudioBus> bus2 = AudioBus::Create(channels, 100);
  EXPECT_EQ(30, buffer.ReadFrames(30, 0, bus2.get()));
  for (int i = 0; i < channels; ++i) {
    VerifyResult(bus1->channel(i),
                 30,
                 static_cast<float>(i),
                 static_cast<float>(channels));
    VerifyResult(bus2->channel(i),
                 30,
                 static_cast<float>(i),
                 static_cast<float>(channels));
  }

  // Peek 10 frames forward
  EXPECT_EQ(5, buffer.PeekFrames(5, 10, 0, bus1.get()));
  for (int i = 0; i < channels; ++i) {
    VerifyResult(bus1->channel(i),
                 5,
                 static_cast<float>(i + 40 * channels),
                 static_cast<float>(channels));
  }

  // Peek to the end of the buffer.
  EXPECT_EQ(30, buffer.frames());
  EXPECT_EQ(30, buffer.PeekFrames(100, 0, 0, bus1.get()));
  EXPECT_EQ(30, buffer.PeekFrames(30, 0, 0, bus1.get()));
}

TEST(AudioBufferQueueTest, Time) {
  const int channels = 2;
  const base::TimeDelta start_time1;
  const base::TimeDelta start_time2 = base::TimeDelta::FromSeconds(30);
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(10);
  AudioBufferQueue buffer;
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);

  // Add two buffers (second one added later):
  //   first:  start=0s,  duration=10s
  //   second: start=30s, duration=10s
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatS16, channels, 1, 1, 10, start_time1, duration));
  EXPECT_EQ(10, buffer.frames());

  // Check starting time.
  EXPECT_EQ(start_time1, buffer.current_time());

  // Read 2 frames, should be 2s in (since duration is 1s per sample).
  EXPECT_EQ(2, buffer.ReadFrames(2, 0, bus.get()));
  EXPECT_EQ(start_time1 + base::TimeDelta::FromSeconds(2),
            buffer.current_time());

  // Skip 2 frames.
  buffer.SeekFrames(2);
  EXPECT_EQ(start_time1 + base::TimeDelta::FromSeconds(4),
            buffer.current_time());

  // Add second buffer for more data.
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatS16, channels, 1, 1, 10, start_time2, duration));
  EXPECT_EQ(16, buffer.frames());

  // Read until almost the end of buffer1.
  EXPECT_EQ(5, buffer.ReadFrames(5, 0, bus.get()));
  EXPECT_EQ(start_time1 + base::TimeDelta::FromSeconds(9),
            buffer.current_time());

  // Read 1 value, so time moved to buffer2.
  EXPECT_EQ(1, buffer.ReadFrames(1, 0, bus.get()));
  EXPECT_EQ(start_time2, buffer.current_time());

  // Read all 10 frames in buffer2, timestamp should be last time from buffer2.
  EXPECT_EQ(10, buffer.ReadFrames(10, 0, bus.get()));
  EXPECT_EQ(start_time2 + base::TimeDelta::FromSeconds(10),
            buffer.current_time());

  // Try to read more frames (which don't exist), timestamp should remain.
  EXPECT_EQ(0, buffer.ReadFrames(5, 0, bus.get()));
  EXPECT_EQ(start_time2 + base::TimeDelta::FromSeconds(10),
            buffer.current_time());
}

TEST(AudioBufferQueueTest, NoTime) {
  const int channels = 2;
  const base::TimeDelta kNoTime = kNoTimestamp();
  AudioBufferQueue buffer;
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);

  // Add two buffers with no timestamps. Time should always be unknown.
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatS16, channels, 1, 1, 10, kNoTime, kNoTime));
  buffer.Append(MakeAudioBuffer<int16>(
      kSampleFormatS16, channels, 1, 1, 10, kNoTime, kNoTime));
  EXPECT_EQ(20, buffer.frames());

  // Check starting time.
  EXPECT_EQ(kNoTime, buffer.current_time());

  // Read 2 frames.
  EXPECT_EQ(2, buffer.ReadFrames(2, 0, bus.get()));
  EXPECT_EQ(kNoTime, buffer.current_time());

  // Skip 2 frames.
  buffer.SeekFrames(2);
  EXPECT_EQ(kNoTime, buffer.current_time());

  // Read until almost the end of buffer1.
  EXPECT_EQ(5, buffer.ReadFrames(5, 0, bus.get()));
  EXPECT_EQ(kNoTime, buffer.current_time());

  // Read 1 value, so time moved to buffer2.
  EXPECT_EQ(1, buffer.ReadFrames(1, 0, bus.get()));
  EXPECT_EQ(kNoTime, buffer.current_time());

  // Read all 10 frames in buffer2.
  EXPECT_EQ(10, buffer.ReadFrames(10, 0, bus.get()));
  EXPECT_EQ(kNoTime, buffer.current_time());

  // Try to read more frames (which don't exist), timestamp should remain.
  EXPECT_EQ(0, buffer.ReadFrames(5, 0, bus.get()));
  EXPECT_EQ(kNoTime, buffer.current_time());
}

}  // namespace media
