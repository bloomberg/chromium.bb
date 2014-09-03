// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_block_fifo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioBlockFifoTest : public testing::Test {
 public:
  AudioBlockFifoTest() {}
  virtual ~AudioBlockFifoTest() {}

  void PushAndVerify(AudioBlockFifo* fifo, int frames_to_push,
                     int channels, int block_frames, int max_frames) {
    const int bytes_per_sample = 2;
    const int data_byte_size = bytes_per_sample * channels * frames_to_push;
    scoped_ptr<uint8[]> data(new uint8[data_byte_size]);
    memset(data.get(), 0, data_byte_size);

    for (int filled_frames = max_frames - fifo->GetUnfilledFrames();
         filled_frames + frames_to_push <= max_frames;) {
      fifo->Push(data.get(), frames_to_push, bytes_per_sample);
      filled_frames += frames_to_push;
      EXPECT_EQ(max_frames - filled_frames, fifo->GetUnfilledFrames());
      EXPECT_EQ(static_cast<int>(filled_frames / block_frames),
                fifo->available_blocks());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioBlockFifoTest);
};

// Verify that construction works as intended.
TEST_F(AudioBlockFifoTest, Construct) {
  const int channels = 6;
  const int frames = 128;
  const int blocks = 4;
  AudioBlockFifo fifo(channels, frames, blocks);
  EXPECT_EQ(0, fifo.available_blocks());
  EXPECT_EQ(frames * blocks, fifo.GetUnfilledFrames());
}

// Pushes audio bus objects to/from a FIFO up to different degrees.
TEST_F(AudioBlockFifoTest, Push) {
  const int channels = 2;
  const int frames = 128;
  const int blocks = 2;
  AudioBlockFifo fifo(channels, frames, blocks);

  // Push frames / 2 of data until FIFO is full.
  PushAndVerify(&fifo, frames / 2, channels, frames, frames * blocks);
  fifo.Clear();

  // Push frames of data until FIFO is full.
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  fifo.Clear();

  // Push 1.5 * frames of data.
  PushAndVerify(&fifo, frames * 1.5, channels, frames, frames * blocks);
  fifo.Clear();
}

// Perform a sequence of Push/Consume calls to different degrees, and verify
// things are correct.
TEST_F(AudioBlockFifoTest, PushAndConsume) {
  const int channels = 2;
  const int frames = 441;
  const int blocks = 4;
  AudioBlockFifo fifo(channels, frames, blocks);
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);

  // Consume 1 block of data.
  const AudioBus* bus = fifo.Consume();
  EXPECT_TRUE(channels == bus->channels());
  EXPECT_TRUE(frames == bus->frames());
  EXPECT_TRUE(fifo.available_blocks() == (blocks - 1));
  EXPECT_TRUE(fifo.GetUnfilledFrames() == frames);

  // Fill it up again.
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);

  // Consume all blocks of data.
  for (int i = 1; i <= blocks; ++i) {
    const AudioBus* bus = fifo.Consume();
    EXPECT_TRUE(channels == bus->channels());
    EXPECT_TRUE(frames == bus->frames());
    EXPECT_TRUE(fifo.GetUnfilledFrames() == frames * i);
    EXPECT_TRUE(fifo.available_blocks() == (blocks - i));
  }
  EXPECT_TRUE(fifo.GetUnfilledFrames() == frames * blocks);
  EXPECT_TRUE(fifo.available_blocks() == 0);

  fifo.Clear();
  int new_push_frames = 128;
  // Change the input frame and try to fill up the FIFO.
  PushAndVerify(&fifo, new_push_frames, channels, frames,
                frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() != 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks -1);

  // Consume all the existing filled blocks of data.
  while (fifo.available_blocks()) {
    const AudioBus* bus = fifo.Consume();
    EXPECT_TRUE(channels == bus->channels());
    EXPECT_TRUE(frames == bus->frames());
  }

  // Since one block of FIFO has not been completely filled up, there should
  // be remaining frames.
  const int number_of_push =
      static_cast<int>(frames * blocks / new_push_frames);
  const int remain_frames = frames * blocks - fifo.GetUnfilledFrames();
  EXPECT_EQ(number_of_push * new_push_frames - frames * (blocks - 1),
            remain_frames);

  // Completely fill up the buffer again.
  new_push_frames = frames * blocks - remain_frames;
  PushAndVerify(&fifo, new_push_frames, channels, frames,
                frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);
}

// Perform a sequence of Push/Consume calls to a 1 block FIFO.
TEST_F(AudioBlockFifoTest, PushAndConsumeOneBlockFifo) {
  static const int channels = 2;
  static const int frames = 441;
  static const int blocks = 1;
  AudioBlockFifo fifo(channels, frames, blocks);
  PushAndVerify(&fifo, frames, channels, frames, frames * blocks);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == 0);
  EXPECT_TRUE(fifo.available_blocks() == blocks);

  // Consume 1 block of data.
  const AudioBus* bus = fifo.Consume();
  EXPECT_TRUE(channels == bus->channels());
  EXPECT_TRUE(frames == bus->frames());
  EXPECT_TRUE(fifo.available_blocks() == 0);
  EXPECT_TRUE(fifo.GetUnfilledFrames() == frames);
}

}  // namespace media
