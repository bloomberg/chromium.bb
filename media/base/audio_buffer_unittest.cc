// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const static int kSampleRate = 44100;

static void VerifyResult(float* channel_data,
                         int frames,
                         float start,
                         float increment) {
  for (int i = 0; i < frames; ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "i=%d/%d start=%f, increment=%f", i, frames, start, increment));
    ASSERT_EQ(channel_data[i], start);
    start += increment;
  }
}

TEST(AudioBufferTest, CopyFrom) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  const int frames = 8;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<uint8>(kSampleFormatU8,
                                                             channel_layout,
                                                             kSampleRate,
                                                             1,
                                                             1,
                                                             frames,
                                                             start_time,
                                                             duration);
  EXPECT_EQ(frames, buffer->frame_count());
  EXPECT_EQ(buffer->timestamp(), start_time);
  EXPECT_EQ(buffer->duration().InSeconds(), frames);
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(AudioBufferTest, CreateEOSBuffer) {
  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateEOSBuffer();
  EXPECT_TRUE(buffer->end_of_stream());
}

TEST(AudioBufferTest, FrameSize) {
  const uint8 kTestData[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                              15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
                              27, 28, 29, 30, 31 };
  const base::TimeDelta kTimestampA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kTimestampB = base::TimeDelta::FromMicroseconds(1234);

  const uint8* const data[] = { kTestData };
  scoped_refptr<AudioBuffer> buffer =
      AudioBuffer::CopyFrom(kSampleFormatU8,
                            CHANNEL_LAYOUT_STEREO,
                            kSampleRate,
                            16,
                            data,
                            kTimestampA,
                            kTimestampB);
  EXPECT_EQ(16, buffer->frame_count());  // 2 channels of 8-bit data

  buffer = AudioBuffer::CopyFrom(kSampleFormatF32,
                                 CHANNEL_LAYOUT_4_0,
                                 kSampleRate,
                                 2,
                                 data,
                                 kTimestampA,
                                 kTimestampB);
  EXPECT_EQ(2, buffer->frame_count());  // now 4 channels of 32-bit data
}

TEST(AudioBufferTest, ReadU8) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 4;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<uint8>(kSampleFormatU8,
                                                             channel_layout,
                                                             kSampleRate,
                                                             128,
                                                             1,
                                                             frames,
                                                             start_time,
                                                             duration);

  // Read all 4 frames from the buffer. Data is interleaved, so ch[0] should be
  // 128, 132, 136, 140, other channels similar. However, values are converted
  // from [0, 255] to [-1.0, 1.0] with a bias of 128. Thus the first buffer
  // value should be 0.0, then 1/127, 2/127, etc.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyResult(bus->channel(0), frames, 0.0f, 4.0f / 127.0f);
  VerifyResult(bus->channel(1), frames, 1.0f / 127.0f, 4.0f / 127.0f);
  VerifyResult(bus->channel(2), frames, 2.0f / 127.0f, 4.0f / 127.0f);
  VerifyResult(bus->channel(3), frames, 3.0f / 127.0f, 4.0f / 127.0f);
}

TEST(AudioBufferTest, ReadS16) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 10;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<int16>(kSampleFormatS16,
                                                             channel_layout,
                                                             kSampleRate,
                                                             1,
                                                             1,
                                                             frames,
                                                             start_time,
                                                             duration);

  // Read 6 frames from the buffer. Data is interleaved, so ch[0] should be 1,
  // 3, 5, 7, 9, 11, and ch[1] should be 2, 4, 6, 8, 10, 12. Data is converted
  // to float from -1.0 to 1.0 based on int16 range.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(6, 0, 0, bus.get());
  VerifyResult(bus->channel(0), 6, 1.0f / kint16max, 2.0f / kint16max);
  VerifyResult(bus->channel(1), 6, 2.0f / kint16max, 2.0f / kint16max);

  // Now read the same data one frame at a time.
  bus = AudioBus::Create(channels, 100);
  for (int i = 0; i < frames; ++i) {
    buffer->ReadFrames(1, i, i, bus.get());
  }
  VerifyResult(bus->channel(0), frames, 1.0f / kint16max, 2.0f / kint16max);
  VerifyResult(bus->channel(1), frames, 2.0f / kint16max, 2.0f / kint16max);
}

TEST(AudioBufferTest, ReadS32) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 6;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<int32>(kSampleFormatS32,
                                                             channel_layout,
                                                             kSampleRate,
                                                             1,
                                                             1,
                                                             frames,
                                                             start_time,
                                                             duration);

  // Read 6 frames from the buffer. Data is interleaved, so ch[0] should be 1,
  // 3, 5, 7, 9, 11, and ch[1] should be 2, 4, 6, 8, 10, 12. Data is converted
  // to float from -1.0 to 1.0 based on int32 range.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyResult(bus->channel(0), frames, 1.0f / kint32max, 2.0f / kint32max);
  VerifyResult(bus->channel(1), frames, 2.0f / kint32max, 2.0f / kint32max);

  // Now read 2 frames starting at frame offset 3. ch[0] should be 7, 9, and
  // ch[1] should be 8, 10.
  buffer->ReadFrames(2, 3, 0, bus.get());
  VerifyResult(bus->channel(0), 2, 7.0f / kint32max, 2.0f / kint32max);
  VerifyResult(bus->channel(1), 2, 8.0f / kint32max, 2.0f / kint32max);
}

TEST(AudioBufferTest, ReadF32) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 20;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<float>(kSampleFormatF32,
                                                             channel_layout,
                                                             kSampleRate,
                                                             1.0f,
                                                             1.0f,
                                                             frames,
                                                             start_time,
                                                             duration);

  // Read first 10 frames from the buffer. F32 is interleaved, so ch[0] should
  // be 1, 3, 5, ... and ch[1] should be 2, 4, 6, ...
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(10, 0, 0, bus.get());
  VerifyResult(bus->channel(0), 10, 1.0f, 2.0f);
  VerifyResult(bus->channel(1), 10, 2.0f, 2.0f);

  // Read second 10 frames.
  bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(10, 10, 0, bus.get());
  VerifyResult(bus->channel(0), 10, 21.0f, 2.0f);
  VerifyResult(bus->channel(1), 10, 22.0f, 2.0f);
}

TEST(AudioBufferTest, ReadS16Planar) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 20;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<int16>(kSampleFormatPlanarS16,
                             channel_layout,
                             kSampleRate,
                             1,
                             1,
                             frames,
                             start_time,
                             duration);

  // Read 6 frames from the buffer. Data is planar, so ch[0] should be 1, 2, 3,
  // 4, 5, 6, and ch[1] should be 21, 22, 23, 24, 25, 26. Data is converted to
  // float from -1.0 to 1.0 based on int16 range.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(6, 0, 0, bus.get());
  VerifyResult(bus->channel(0), 6, 1.0f / kint16max, 1.0f / kint16max);
  VerifyResult(bus->channel(1), 6, 21.0f / kint16max, 1.0f / kint16max);

  // Read all the frames backwards, one by one. ch[0] should be 20, 19, ...
  bus = AudioBus::Create(channels, 100);
  for (int i = 0; i < frames; ++i) {
    buffer->ReadFrames(1, frames - i - 1, i, bus.get());
  }
  VerifyResult(bus->channel(0), frames, 20.0f / kint16max, -1.0f / kint16max);
  VerifyResult(bus->channel(1), frames, 40.0f / kint16max, -1.0f / kint16max);

  // Read 0 frames with different offsets. Existing data in AudioBus should be
  // unchanged.
  buffer->ReadFrames(0, 0, 0, bus.get());
  buffer->ReadFrames(0, 0, 10, bus.get());
  buffer->ReadFrames(0, 10, 0, bus.get());
  VerifyResult(bus->channel(0), frames, 20.0f / kint16max, -1.0f / kint16max);
  VerifyResult(bus->channel(1), frames, 40.0f / kint16max, -1.0f / kint16max);
}

TEST(AudioBufferTest, ReadF32Planar) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 100;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<float>(kSampleFormatPlanarF32,
                             channel_layout,
                             kSampleRate,
                             1.0f,
                             1.0f,
                             frames,
                             start_time,
                             duration);

  // Read all 100 frames from the buffer. F32 is planar, so ch[0] should be 1,
  // 2, 3, 4, ..., ch[1] should be 101, 102, 103, ..., and so on for all 4
  // channels.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyResult(bus->channel(0), frames, 1.0f, 1.0f);
  VerifyResult(bus->channel(1), frames, 101.0f, 1.0f);
  VerifyResult(bus->channel(2), frames, 201.0f, 1.0f);
  VerifyResult(bus->channel(3), frames, 301.0f, 1.0f);

  // Now read 20 frames from the middle of the buffer.
  bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(20, 50, 0, bus.get());
  VerifyResult(bus->channel(0), 20, 51.0f, 1.0f);
  VerifyResult(bus->channel(1), 20, 151.0f, 1.0f);
  VerifyResult(bus->channel(2), 20, 251.0f, 1.0f);
  VerifyResult(bus->channel(3), 20, 351.0f, 1.0f);
}

TEST(AudioBufferTest, EmptyBuffer) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 100;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer = AudioBuffer::CreateEmptyBuffer(
      channel_layout, kSampleRate, frames, start_time, duration);
  EXPECT_EQ(frames, buffer->frame_count());
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames, buffer->duration().InSeconds());
  EXPECT_FALSE(buffer->end_of_stream());

  // Read all 100 frames from the buffer. All data should be 0.
  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(frames, 0, 0, bus.get());
  VerifyResult(bus->channel(0), frames, 0.0f, 0.0f);
  VerifyResult(bus->channel(1), frames, 0.0f, 0.0f);
  VerifyResult(bus->channel(2), frames, 0.0f, 0.0f);
  VerifyResult(bus->channel(3), frames, 0.0f, 0.0f);
}

TEST(AudioBufferTest, Trim) {
  const ChannelLayout channel_layout = CHANNEL_LAYOUT_4_0;
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  const int frames = 100;
  const base::TimeDelta start_time;
  const base::TimeDelta duration = base::TimeDelta::FromSeconds(frames);
  scoped_refptr<AudioBuffer> buffer =
      MakeAudioBuffer<float>(kSampleFormatPlanarF32,
                             channel_layout,
                             kSampleRate,
                             1.0f,
                             1.0f,
                             frames,
                             start_time,
                             duration);
  EXPECT_EQ(frames, buffer->frame_count());
  EXPECT_EQ(start_time, buffer->timestamp());
  EXPECT_EQ(frames, buffer->duration().InSeconds());

  scoped_ptr<AudioBus> bus = AudioBus::Create(channels, 100);
  buffer->ReadFrames(20, 0, 0, bus.get());
  VerifyResult(bus->channel(0), 20, 1.0f, 1.0f);

  // Trim off 10 frames from the start.
  buffer->TrimStart(10);
  EXPECT_EQ(buffer->frame_count(), frames - 10);
  EXPECT_EQ(buffer->timestamp(), start_time + base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(buffer->duration(), base::TimeDelta::FromSeconds(90));
  buffer->ReadFrames(20, 0, 0, bus.get());
  VerifyResult(bus->channel(0), 20, 11.0f, 1.0f);

  // Trim off 10 frames from the end.
  buffer->TrimEnd(10);
  EXPECT_EQ(buffer->frame_count(), frames - 20);
  EXPECT_EQ(buffer->timestamp(), start_time + base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(buffer->duration(), base::TimeDelta::FromSeconds(80));
  buffer->ReadFrames(20, 0, 0, bus.get());
  VerifyResult(bus->channel(0), 20, 11.0f, 1.0f);

  // Trim off 50 more from the start.
  buffer->TrimStart(50);
  EXPECT_EQ(buffer->frame_count(), frames - 70);
  EXPECT_EQ(buffer->timestamp(), start_time + base::TimeDelta::FromSeconds(60));
  EXPECT_EQ(buffer->duration(), base::TimeDelta::FromSeconds(30));
  buffer->ReadFrames(10, 0, 0, bus.get());
  VerifyResult(bus->channel(0), 10, 61.0f, 1.0f);

  // Trim off the last 30 frames.
  buffer->TrimEnd(30);
  EXPECT_EQ(buffer->frame_count(), 0);
  EXPECT_EQ(buffer->timestamp(), start_time + base::TimeDelta::FromSeconds(60));
  EXPECT_EQ(buffer->duration(), base::TimeDelta::FromSeconds(0));
}

}  // namespace media
