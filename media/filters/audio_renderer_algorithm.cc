// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_algorithm.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_util.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"

namespace media {

// The starting size in frames for |audio_buffer_|. Previous usage maintained a
// queue of 16 AudioBuffers, each of 512 frames. This worked well, so we
// maintain this number of frames.
static const int kStartingBufferSizeInFrames = 16 * 512;

// The maximum size in frames for the |audio_buffer_|. Arbitrarily determined.
// This number represents 3 seconds of 96kHz/16 bit 7.1 surround sound.
static const int kMaxBufferSizeInFrames = 3 * 96000;

// Duration of audio segments used for crossfading (in seconds).
static const double kWindowDuration = 0.08;

// Duration of crossfade between audio segments (in seconds).
static const double kCrossfadeDuration = 0.008;

// Max/min supported playback rates for fast/slow audio. Audio outside of these
// ranges are muted.
// Audio at these speeds would sound better under a frequency domain algorithm.
static const float kMinPlaybackRate = 0.5f;
static const float kMaxPlaybackRate = 4.0f;

AudioRendererAlgorithm::AudioRendererAlgorithm()
    : channels_(0),
      samples_per_second_(0),
      playback_rate_(0),
      frames_in_crossfade_(0),
      index_into_window_(0),
      crossfade_frame_number_(0),
      muted_(false),
      muted_partial_frame_(0),
      window_size_(0),
      capacity_(kStartingBufferSizeInFrames) {
}

AudioRendererAlgorithm::~AudioRendererAlgorithm() {}

void AudioRendererAlgorithm::Initialize(float initial_playback_rate,
                                        const AudioParameters& params) {
  CHECK(params.IsValid());

  channels_ = params.channels();
  samples_per_second_ = params.sample_rate();
  SetPlaybackRate(initial_playback_rate);

  window_size_ = samples_per_second_ * kWindowDuration;
  frames_in_crossfade_ = samples_per_second_ * kCrossfadeDuration;
  crossfade_buffer_ = AudioBus::Create(channels_, frames_in_crossfade_);
}

int AudioRendererAlgorithm::FillBuffer(AudioBus* dest, int requested_frames) {
  if (playback_rate_ == 0)
    return 0;

  // Optimize the |muted_| case to issue a single clear instead of performing
  // the full crossfade and clearing each crossfaded frame.
  if (muted_) {
    int frames_to_render =
        std::min(static_cast<int>(audio_buffer_.frames() / playback_rate_),
                 requested_frames);

    // Compute accurate number of frames to actually skip in the source data.
    // Includes the leftover partial frame from last request. However, we can
    // only skip over complete frames, so a partial frame may remain for next
    // time.
    muted_partial_frame_ += frames_to_render * playback_rate_;
    int seek_frames = static_cast<int>(muted_partial_frame_);
    dest->ZeroFrames(frames_to_render);
    audio_buffer_.SeekFrames(seek_frames);

    // Determine the partial frame that remains to be skipped for next call. If
    // the user switches back to playing, it may be off time by this partial
    // frame, which would be undetectable. If they subsequently switch to
    // another playback rate that mutes, the code will attempt to line up the
    // frames again.
    muted_partial_frame_ -= seek_frames;
    return frames_to_render;
  }

  int slower_step = ceil(window_size_ * playback_rate_);
  int faster_step = ceil(window_size_ / playback_rate_);

  // Optimize the most common |playback_rate_| ~= 1 case to use a single copy
  // instead of copying frame by frame.
  if (window_size_ <= faster_step && slower_step >= window_size_) {
    const int frames_to_copy =
        std::min(audio_buffer_.frames(), requested_frames);
    const int frames_read = audio_buffer_.ReadFrames(frames_to_copy, 0, dest);
    DCHECK_EQ(frames_read, frames_to_copy);
    return frames_read;
  }

  int total_frames_rendered = 0;
  while (total_frames_rendered < requested_frames) {
    if (index_into_window_ >= window_size_)
      ResetWindow();

    int rendered_frames = 0;
    if (window_size_ > faster_step) {
      rendered_frames =
          OutputFasterPlayback(dest,
                               total_frames_rendered,
                               requested_frames - total_frames_rendered,
                               window_size_,
                               faster_step);
    } else if (slower_step < window_size_) {
      rendered_frames =
          OutputSlowerPlayback(dest,
                               total_frames_rendered,
                               requested_frames - total_frames_rendered,
                               slower_step,
                               window_size_);
    } else {
      NOTREACHED();
    }

    if (rendered_frames == 0)
      break;

    total_frames_rendered += rendered_frames;
  }
  return total_frames_rendered;
}

void AudioRendererAlgorithm::ResetWindow() {
  DCHECK_LE(index_into_window_, window_size_);
  index_into_window_ = 0;
  crossfade_frame_number_ = 0;
}

int AudioRendererAlgorithm::OutputFasterPlayback(AudioBus* dest,
                                                 int dest_offset,
                                                 int requested_frames,
                                                 int input_step,
                                                 int output_step) {
  // Ensure we don't run into OOB read/write situation.
  CHECK_GT(input_step, output_step);
  DCHECK_LT(index_into_window_, window_size_);
  DCHECK_GT(playback_rate_, 1.0);
  DCHECK(!muted_);

  if (audio_buffer_.frames() < 1)
    return 0;

  // The audio data is output in a series of windows. For sped-up playback,
  // the window is comprised of the following phases:
  //
  //  a) Output raw data.
  //  b) Save bytes for crossfade in |crossfade_buffer_|.
  //  c) Drop data.
  //  d) Output crossfaded audio leading up to the next window.
  //
  // The duration of each phase is computed below based on the |window_size_|
  // and |playback_rate_|.
  DCHECK_LE(frames_in_crossfade_, output_step);

  // This is the index of the end of phase a, beginning of phase b.
  int outtro_crossfade_begin = output_step - frames_in_crossfade_;

  // This is the index of the end of phase b, beginning of phase c.
  int outtro_crossfade_end = output_step;

  // This is the index of the end of phase c, beginning of phase d.
  // This phase continues until |index_into_window_| reaches |window_size_|, at
  // which point the window restarts.
  int intro_crossfade_begin = input_step - frames_in_crossfade_;

  // a) Output raw frames if we haven't reached the crossfade section.
  if (index_into_window_ < outtro_crossfade_begin) {
    // Read as many frames as we can and return the count. If it's not enough,
    // we will get called again.
    const int frames_to_copy =
        std::min(requested_frames, outtro_crossfade_begin - index_into_window_);
    int copied = audio_buffer_.ReadFrames(frames_to_copy, dest_offset, dest);
    index_into_window_ += copied;
    return copied;
  }

  // b) Save outtro crossfade frames into intermediate buffer, but do not output
  //    anything to |dest|.
  if (index_into_window_ < outtro_crossfade_end) {
    // This phase only applies if there are bytes to crossfade.
    DCHECK_GT(frames_in_crossfade_, 0);
    int crossfade_start = index_into_window_ - outtro_crossfade_begin;
    int crossfade_count = outtro_crossfade_end - index_into_window_;
    int copied = audio_buffer_.ReadFrames(
        crossfade_count, crossfade_start, crossfade_buffer_.get());
    index_into_window_ += copied;

    // Did we get all the frames we need? If not, return and let subsequent
    // calls try to get the rest.
    if (copied != crossfade_count)
      return 0;
  }

  // c) Drop frames until we reach the intro crossfade section.
  if (index_into_window_ < intro_crossfade_begin) {
    // Check if there is enough data to skip all the frames needed. If not,
    // return 0 and let subsequent calls try to skip it all.
    int seek_frames = intro_crossfade_begin - index_into_window_;
    if (audio_buffer_.frames() < seek_frames)
      return 0;
    audio_buffer_.SeekFrames(seek_frames);

    // We've dropped all the frames that need to be dropped.
    index_into_window_ += seek_frames;
  }

  // d) Crossfade and output a frame, as long as we have data.
  if (audio_buffer_.frames() < 1)
    return 0;
  DCHECK_GT(frames_in_crossfade_, 0);
  DCHECK_LT(index_into_window_, window_size_);

  int offset_into_buffer = index_into_window_ - intro_crossfade_begin;
  int copied = audio_buffer_.ReadFrames(1, dest_offset, dest);
  DCHECK_EQ(copied, 1);
  CrossfadeFrame(crossfade_buffer_.get(),
                 offset_into_buffer,
                 dest,
                 dest_offset,
                 offset_into_buffer);
  index_into_window_ += copied;
  return copied;
}

int AudioRendererAlgorithm::OutputSlowerPlayback(AudioBus* dest,
                                                 int dest_offset,
                                                 int requested_frames,
                                                 int input_step,
                                                 int output_step) {
  // Ensure we don't run into OOB read/write situation.
  CHECK_LT(input_step, output_step);
  DCHECK_LT(index_into_window_, window_size_);
  DCHECK_LT(playback_rate_, 1.0);
  DCHECK_NE(playback_rate_, 0);
  DCHECK(!muted_);

  if (audio_buffer_.frames() < 1)
    return 0;

  // The audio data is output in a series of windows. For slowed down playback,
  // the window is comprised of the following phases:
  //
  //  a) Output raw data.
  //  b) Output and save bytes for crossfade in |crossfade_buffer_|.
  //  c) Output* raw data.
  //  d) Output* crossfaded audio leading up to the next window.
  //
  // * Phases c) and d) do not progress |audio_buffer_|'s cursor so that the
  // |audio_buffer_|'s cursor is in the correct place for the next window.
  //
  // The duration of each phase is computed below based on the |window_size_|
  // and |playback_rate_|.
  DCHECK_LE(frames_in_crossfade_, input_step);

  // This is the index of the end of phase a, beginning of phase b.
  int intro_crossfade_begin = input_step - frames_in_crossfade_;

  // This is the index of the end of phase b, beginning of phase c.
  int intro_crossfade_end = input_step;

  // This is the index of the end of phase c,  beginning of phase d.
  // This phase continues until |index_into_window_| reaches |window_size_|, at
  // which point the window restarts.
  int outtro_crossfade_begin = output_step - frames_in_crossfade_;

  // a) Output raw frames.
  if (index_into_window_ < intro_crossfade_begin) {
    // Read as many frames as we can and return the count. If it's not enough,
    // we will get called again.
    const int frames_to_copy =
        std::min(requested_frames, intro_crossfade_begin - index_into_window_);
    int copied = audio_buffer_.ReadFrames(frames_to_copy, dest_offset, dest);
    index_into_window_ += copied;
    return copied;
  }

  // b) Save the raw frames for the intro crossfade section, then copy the
  //    same frames to |dest|.
  if (index_into_window_ < intro_crossfade_end) {
    const int frames_to_copy =
        std::min(requested_frames, intro_crossfade_end - index_into_window_);
    int offset = index_into_window_ - intro_crossfade_begin;
    int copied = audio_buffer_.ReadFrames(
        frames_to_copy, offset, crossfade_buffer_.get());
    crossfade_buffer_->CopyPartialFramesTo(offset, copied, dest_offset, dest);
    index_into_window_ += copied;
    return copied;
  }

  // c) Output a raw frame into |dest| without advancing the |audio_buffer_|
  //    cursor.
  int audio_buffer_offset = index_into_window_ - intro_crossfade_end;
  DCHECK_GE(audio_buffer_offset, 0);
  if (audio_buffer_.frames() <= audio_buffer_offset)
    return 0;
  int copied =
      audio_buffer_.PeekFrames(1, audio_buffer_offset, dest_offset, dest);
  DCHECK_EQ(1, copied);

  // d) Crossfade the next frame of |crossfade_buffer_| into |dest| if we've
  //    reached the outtro crossfade section of the window.
  if (index_into_window_ >= outtro_crossfade_begin) {
    int offset_into_crossfade_buffer =
        index_into_window_ - outtro_crossfade_begin;
    CrossfadeFrame(dest,
                   dest_offset,
                   crossfade_buffer_.get(),
                   offset_into_crossfade_buffer,
                   offset_into_crossfade_buffer);
  }

  index_into_window_ += copied;
  return copied;
}

void AudioRendererAlgorithm::CrossfadeFrame(AudioBus* intro,
                                            int intro_offset,
                                            AudioBus* outtro,
                                            int outtro_offset,
                                            int fade_offset) {
  float crossfade_ratio =
      static_cast<float>(fade_offset) / frames_in_crossfade_;
  for (int channel = 0; channel < channels_; ++channel) {
    outtro->channel(channel)[outtro_offset] =
        (1.0f - crossfade_ratio) * intro->channel(channel)[intro_offset] +
        (crossfade_ratio) * outtro->channel(channel)[outtro_offset];
  }
}

void AudioRendererAlgorithm::SetPlaybackRate(float new_rate) {
  DCHECK_GE(new_rate, 0);
  playback_rate_ = new_rate;
  muted_ =
      playback_rate_ < kMinPlaybackRate || playback_rate_ > kMaxPlaybackRate;

  ResetWindow();
}

void AudioRendererAlgorithm::FlushBuffers() {
  ResetWindow();

  // Clear the queue of decoded packets (releasing the buffers).
  audio_buffer_.Clear();
}

base::TimeDelta AudioRendererAlgorithm::GetTime() {
  return audio_buffer_.current_time();
}

void AudioRendererAlgorithm::EnqueueBuffer(
    const scoped_refptr<AudioBuffer>& buffer_in) {
  DCHECK(!buffer_in->end_of_stream());
  audio_buffer_.Append(buffer_in);
}

bool AudioRendererAlgorithm::IsQueueFull() {
  return audio_buffer_.frames() >= capacity_;
}

void AudioRendererAlgorithm::IncreaseQueueCapacity() {
  capacity_ = std::min(2 * capacity_, kMaxBufferSizeInFrames);
}

}  // namespace media
