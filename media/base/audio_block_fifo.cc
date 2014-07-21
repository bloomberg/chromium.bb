// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_block_fifo.h"

#include "base/logging.h"

namespace media {

AudioBlockFifo::AudioBlockFifo(int channels, int frames, int blocks)
    : block_frames_(frames),
      write_block_(0),
      read_block_(0),
      available_blocks_(0),
      write_pos_(0) {
  // Create |blocks| of audio buses and push them to the containers.
  audio_blocks_.reserve(blocks);
  for (int i = 0; i < blocks; ++i) {
    scoped_ptr<AudioBus> audio_bus = AudioBus::Create(channels, frames);
    audio_blocks_.push_back(audio_bus.release());
  }
}

AudioBlockFifo::~AudioBlockFifo() {}

void AudioBlockFifo::Push(const void* source,
                          int frames,
                          int bytes_per_sample) {
  DCHECK(source);
  DCHECK_GT(frames, 0);
  DCHECK_GT(bytes_per_sample, 0);
  DCHECK_LT(available_blocks_, static_cast<int>(audio_blocks_.size()));

  const uint8* source_ptr = static_cast<const uint8*>(source);
  int frames_to_push = frames;
  while (frames_to_push) {
    // Get the current write block.
    AudioBus* current_block = audio_blocks_[write_block_];

    // Figure out what segment sizes we need when adding the new content to
    // the FIFO.
    const int push_frames =
        std::min(block_frames_ - write_pos_, frames_to_push);

    // Deinterleave the content to the FIFO and update the |write_pos_|.
    current_block->FromInterleavedPartial(
        source_ptr, write_pos_, push_frames, bytes_per_sample);
    write_pos_ = (write_pos_ + push_frames) % block_frames_;
    if (!write_pos_) {
      // The current block is completely filled, increment |write_block_| and
      // |available_blocks_|.
      write_block_ = (write_block_ + 1) % audio_blocks_.size();
      ++available_blocks_;
    }

    source_ptr += push_frames * bytes_per_sample * current_block->channels();
    frames_to_push -= push_frames;
    DCHECK_GE(frames_to_push, 0);
  }
}

const AudioBus* AudioBlockFifo::Consume() {
  DCHECK(available_blocks_);
  AudioBus* audio_bus = audio_blocks_[read_block_];
  read_block_ = (read_block_ + 1) % audio_blocks_.size();
  --available_blocks_;
  return audio_bus;
}

void AudioBlockFifo::Clear() {
  write_pos_ = 0;
  write_block_ = 0;
  read_block_ = 0;
  available_blocks_ = 0;
}

int AudioBlockFifo::GetAvailableFrames() const {
  return available_blocks_ * block_frames_ + write_pos_;
}

int AudioBlockFifo::GetUnfilledFrames() const {
  const int unfilled_frames =
      (audio_blocks_.size() - available_blocks_) * block_frames_ - write_pos_;
  DCHECK_GE(unfilled_frames, 0);
  return unfilled_frames;
}

}  // namespace media
