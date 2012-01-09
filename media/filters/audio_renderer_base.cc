// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_base.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "media/base/filter_host.h"

namespace media {

AudioRendererBase::AudioRendererBase()
    : state_(kUninitialized),
      pending_read_(false),
      recieved_end_of_stream_(false),
      rendered_end_of_stream_(false),
      read_cb_(base::Bind(&AudioRendererBase::DecodedAudioReady,
                          base::Unretained(this))) {
}

AudioRendererBase::~AudioRendererBase() {
  // Stop() should have been called and |algorithm_| should have been destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!algorithm_.get());
}

void AudioRendererBase::Play(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  state_ = kPlaying;
  callback.Run();
}

void AudioRendererBase::Pause(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kPlaying || state_ == kUnderflow || state_ == kRebuffering);
  pause_callback_ = callback;
  state_ = kPaused;

  // Pause only when we've completed our pending read.
  if (!pending_read_) {
    pause_callback_.Run();
    pause_callback_.Reset();
  } else {
    state_ = kPaused;
  }
}

void AudioRendererBase::Stop(const base::Closure& callback) {
  OnStop();
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;
    algorithm_.reset(NULL);
  }
  if (!callback.is_null()) {
    callback.Run();
  }
}

void AudioRendererBase::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";
  DCHECK(seek_cb_.is_null());
  state_ = kSeeking;
  seek_cb_ = cb;
  seek_timestamp_ = time;

  // Throw away everything and schedule our reads.
  last_fill_buffer_time_ = base::TimeDelta();
  recieved_end_of_stream_ = false;
  rendered_end_of_stream_ = false;

  // |algorithm_| will request more reads.
  algorithm_->FlushBuffers();
}

void AudioRendererBase::Initialize(AudioDecoder* decoder,
                                   const base::Closure& init_callback,
                                   const base::Closure& underflow_callback) {
  DCHECK(decoder);
  DCHECK(!init_callback.is_null());
  DCHECK(!underflow_callback.is_null());
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;
  underflow_callback_ = underflow_callback;

  // Create a callback so our algorithm can request more reads.
  base::Closure cb = base::Bind(&AudioRendererBase::ScheduleRead_Locked, this);

  // Construct the algorithm.
  algorithm_.reset(new AudioRendererAlgorithmBase());

  // Initialize our algorithm with media properties, initial playback rate,
  // and a callback to request more reads from the data source.
  ChannelLayout channel_layout = decoder_->channel_layout();
  int channels = ChannelLayoutToChannelCount(channel_layout);
  int bits_per_channel = decoder_->bits_per_channel();
  int sample_rate = decoder_->samples_per_second();
  algorithm_->Initialize(channels, sample_rate, bits_per_channel, 0.0f, cb);

  // Give the subclass an opportunity to initialize itself.
  if (!OnInitialize(bits_per_channel, channel_layout, sample_rate)) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    init_callback.Run();
    return;
  }

  // Finally, execute the start callback.
  state_ = kPaused;
  init_callback.Run();
}

bool AudioRendererBase::HasEnded() {
  base::AutoLock auto_lock(lock_);
  if (rendered_end_of_stream_) {
    DCHECK(algorithm_->IsQueueEmpty())
        << "Audio queue should be empty if we have rendered end of stream";
  }
  return recieved_end_of_stream_ && rendered_end_of_stream_;
}

void AudioRendererBase::ResumeAfterUnderflow(bool buffer_more_audio) {
  base::AutoLock auto_lock(lock_);
  if (state_ == kUnderflow) {
    if (buffer_more_audio)
      algorithm_->IncreaseQueueCapacity();

    state_ = kRebuffering;
  }
}

void AudioRendererBase::DecodedAudioReady(scoped_refptr<Buffer> buffer) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying ||
         state_ == kUnderflow || state_ == kRebuffering || state_ == kStopped);

  CHECK(pending_read_);
  pending_read_ = false;

  // TODO(scherkus): this happens due to a race, primarily because Stop() is a
  // synchronous call when it should be asynchronous and accept a callback.
  // Refer to http://crbug.com/16059
  if (state_ == kStopped) {
    return;
  }

  // Don't enqueue an end-of-stream buffer because it has no data, otherwise
  // discard decoded audio data until we reach our desired seek timestamp.
  if (buffer->IsEndOfStream()) {
    recieved_end_of_stream_ = true;

    // Transition to kPlaying if we are currently handling an underflow since no
    // more data will be arriving.
    if (state_ == kUnderflow || state_ == kRebuffering)
      state_ = kPlaying;
  } else if (state_ == kSeeking && !buffer->IsEndOfStream() &&
             (buffer->GetTimestamp() + buffer->GetDuration()) <
                 seek_timestamp_) {
    ScheduleRead_Locked();
  } else {
    // Note: Calling this may schedule more reads.
    algorithm_->EnqueueBuffer(buffer);
  }

  // Check for our preroll complete condition.
  if (state_ == kSeeking) {
    DCHECK(!seek_cb_.is_null());
    if (algorithm_->IsQueueFull() || recieved_end_of_stream_) {
      // Transition into paused whether we have data in |algorithm_| or not.
      // FillBuffer() will play silence if there's nothing to fill.
      state_ = kPaused;
      ResetAndRunCB(&seek_cb_, PIPELINE_OK);
    }
  } else if (state_ == kPaused && !pending_read_) {
    // No more pending read!  We're now officially "paused".
    if (!pause_callback_.is_null()) {
      pause_callback_.Run();
      pause_callback_.Reset();
    }
  }
}

uint32 AudioRendererBase::FillBuffer(uint8* dest,
                                     uint32 dest_len,
                                     const base::TimeDelta& playback_delay,
                                     bool buffers_empty) {
  // The timestamp of the last buffer written during the last call to
  // FillBuffer().
  base::TimeDelta last_fill_buffer_time;
  size_t dest_written = 0;
  base::Closure underflow_cb;
  {
    base::AutoLock auto_lock(lock_);

    if (state_ == kRebuffering && algorithm_->IsQueueFull())
      state_ = kPlaying;

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      // TODO(scherkus): To keep the audio hardware busy we write at most 8k of
      // zeros.  This gets around the tricky situation of pausing and resuming
      // the audio IPC layer in Chrome.  Ideally, we should return zero and then
      // the subclass can restart the conversation.
      //
      // This should get handled by the subclass http://crbug.com/106600
      const uint32 kZeroLength = 8192;
      dest_written = std::min(kZeroLength, dest_len);
      memset(dest, 0, dest_written);
      return dest_written;
    }

    // Save a local copy of last fill buffer time and reset the member.
    last_fill_buffer_time = last_fill_buffer_time_;
    last_fill_buffer_time_ = base::TimeDelta();

    // Use three conditions to determine the end of playback:
    // 1. Algorithm has no audio data. (algorithm_->IsQueueEmpty() == true)
    // 2. Browser process has no audio data. (buffers_empty == true)
    // 3. We've recieved an end of stream buffer.
    //    (recieved_end_of_stream_ == true)
    //
    // Three conditions determine when an underflow occurs:
    // 1. Algorithm has no audio data.
    // 2. Currently in the kPlaying state.
    // 3. Have not received an end of stream buffer.
    if (algorithm_->IsQueueEmpty()) {
      if (buffers_empty && recieved_end_of_stream_) {
        if (!rendered_end_of_stream_) {
          rendered_end_of_stream_ = true;
          host()->NotifyEnded();
        }
      } else if (state_ == kPlaying && !recieved_end_of_stream_) {
        state_ = kUnderflow;
        underflow_cb = underflow_callback_;
      }
    } else {
      // Otherwise fill the buffer.
      dest_written = algorithm_->FillBuffer(dest, dest_len);
    }

    // Get the current time.
    last_fill_buffer_time_ = algorithm_->GetTime();
  }

  // Update the pipeline's time if it was set last time.
  if (last_fill_buffer_time.InMicroseconds() > 0 &&
      (last_fill_buffer_time != last_fill_buffer_time_ ||
       (last_fill_buffer_time - playback_delay) > host()->GetTime())) {
    // Adjust the |last_fill_buffer_time| with the playback delay.
    // TODO(hclam): If there is a playback delay, the pipeline would not be
    // updated with a correct timestamp when the stream is played at the very
    // end since we use decoded packets to trigger time updates. A better
    // solution is to start a timer when an audio packet is decoded to allow
    // finer time update events.
    last_fill_buffer_time -= playback_delay;
    host()->SetTime(last_fill_buffer_time);
  }

  if (!underflow_cb.is_null())
    underflow_cb.Run();

  return dest_written;
}

void AudioRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  if (pending_read_)
    return;
  pending_read_ = true;
  decoder_->Read(read_cb_);
}

void AudioRendererBase::SetPlaybackRate(float playback_rate) {
  algorithm_->SetPlaybackRate(playback_rate);
}

float AudioRendererBase::GetPlaybackRate() {
  return algorithm_->playback_rate();
}

}  // namespace media
