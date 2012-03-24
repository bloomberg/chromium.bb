// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_base.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "media/base/filter_host.h"

namespace media {

AudioRendererBase::AudioRendererBase()
    : state_(kUninitialized),
      pending_read_(false),
      received_end_of_stream_(false),
      rendered_end_of_stream_(false),
      bytes_per_frame_(0),
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
  pause_cb_ = callback;
  state_ = kPaused;

  // Pause only when we've completed our pending read.
  if (!pending_read_) {
    pause_cb_.Run();
    pause_cb_.Reset();
  } else {
    state_ = kPaused;
  }
}

void AudioRendererBase::Flush(const base::Closure& callback) {
  decoder_->Reset(callback);
}

void AudioRendererBase::Stop(const base::Closure& callback) {
  OnStop();
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;
    algorithm_.reset(NULL);
    time_cb_.Reset();
    underflow_cb_.Reset();
  }
  if (!callback.is_null()) {
    callback.Run();
  }
}

void AudioRendererBase::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  DCHECK(!pending_read_) << "Pending read must complete before seeking";
  DCHECK(pause_cb_.is_null());
  DCHECK(seek_cb_.is_null());
  state_ = kSeeking;
  seek_cb_ = cb;
  seek_timestamp_ = time;

  // Throw away everything and schedule our reads.
  last_fill_buffer_time_ = base::TimeDelta();
  received_end_of_stream_ = false;
  rendered_end_of_stream_ = false;

  // |algorithm_| will request more reads.
  algorithm_->FlushBuffers();
}

void AudioRendererBase::Initialize(const scoped_refptr<AudioDecoder>& decoder,
                                   const PipelineStatusCB& init_cb,
                                   const base::Closure& underflow_cb,
                                   const TimeCB& time_cb) {
  DCHECK(decoder);
  DCHECK(!init_cb.is_null());
  DCHECK(!underflow_cb.is_null());
  DCHECK(!time_cb.is_null());
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;
  underflow_cb_ = underflow_cb;
  time_cb_ = time_cb;

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
  // TODO(vrk): Add method to AudioDecoder to compute bytes per frame.
  bytes_per_frame_ = channels * bits_per_channel / 8;

  bool config_ok = algorithm_->ValidateConfig(channels, sample_rate,
                                              bits_per_channel);
  if (config_ok)
    algorithm_->Initialize(channels, sample_rate, bits_per_channel, 0.0f, cb);

  // Give the subclass an opportunity to initialize itself.
  if (!config_ok || !OnInitialize(bits_per_channel, channel_layout,
                                  sample_rate)) {
    init_cb.Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  // Finally, execute the start callback.
  state_ = kPaused;
  init_cb.Run(PIPELINE_OK);
}

bool AudioRendererBase::HasEnded() {
  base::AutoLock auto_lock(lock_);
  DCHECK(!rendered_end_of_stream_ || algorithm_->NeedsMoreData());

  return received_end_of_stream_ && rendered_end_of_stream_;
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

  if (buffer && buffer->IsEndOfStream()) {
    received_end_of_stream_ = true;

    // Transition to kPlaying if we are currently handling an underflow since
    // no more data will be arriving.
    if (state_ == kUnderflow || state_ == kRebuffering)
      state_ = kPlaying;
  }

  switch (state_) {
    case kUninitialized:
      NOTREACHED();
      return;
    case kPaused:
      if (buffer && !buffer->IsEndOfStream())
        algorithm_->EnqueueBuffer(buffer);
      DCHECK(!pending_read_);
      base::ResetAndReturn(&pause_cb_).Run();
      return;
    case kSeeking:
      if (IsBeforeSeekTime(buffer)) {
        ScheduleRead_Locked();
        return;
      }
      if (buffer && !buffer->IsEndOfStream()) {
        algorithm_->EnqueueBuffer(buffer);
        if (!algorithm_->IsQueueFull())
          return;
      }
      state_ = kPaused;
      base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
      return;
    case kPlaying:
    case kUnderflow:
    case kRebuffering:
      if (buffer && !buffer->IsEndOfStream())
        algorithm_->EnqueueBuffer(buffer);
      return;
    case kStopped:
      return;
  }
}

uint32 AudioRendererBase::FillBuffer(uint8* dest,
                                     uint32 requested_frames,
                                     const base::TimeDelta& playback_delay) {
  // The timestamp of the last buffer written during the last call to
  // FillBuffer().
  base::TimeDelta last_fill_buffer_time;
  size_t frames_written = 0;
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
      size_t zeros_to_write =
          std::min(kZeroLength, requested_frames * bytes_per_frame_);
      memset(dest, 0, zeros_to_write);
      return zeros_to_write / bytes_per_frame_;
    }

    // Save a local copy of last fill buffer time and reset the member.
    last_fill_buffer_time = last_fill_buffer_time_;
    last_fill_buffer_time_ = base::TimeDelta();

    // Use three conditions to determine the end of playback:
    // 1. Algorithm needs more audio data.
    // 2. We've received an end of stream buffer.
    //    (received_end_of_stream_ == true)
    // 3. Browser process has no audio data being played.
    //    There is no way to check that condition that would work for all
    //    derived classes, so call virtual method that would either render
    //    end of stream or schedule such rendering.
    //
    // Three conditions determine when an underflow occurs:
    // 1. Algorithm has no audio data.
    // 2. Currently in the kPlaying state.
    // 3. Have not received an end of stream buffer.
    if (algorithm_->NeedsMoreData()) {
      if (received_end_of_stream_) {
        OnRenderEndOfStream();
      } else if (state_ == kPlaying) {
        state_ = kUnderflow;
        underflow_cb = underflow_cb_;
      }
    } else {
      // Otherwise fill the buffer.
      frames_written = algorithm_->FillBuffer(dest, requested_frames);
    }

    // Get the current time.
    last_fill_buffer_time_ = algorithm_->GetTime();
  }

  // Update the pipeline's time if it was set last time.
  base::TimeDelta new_current_time = last_fill_buffer_time - playback_delay;
  if (last_fill_buffer_time.InMicroseconds() > 0 &&
      (last_fill_buffer_time != last_fill_buffer_time_ ||
       new_current_time > host()->GetTime())) {
    time_cb_.Run(new_current_time, last_fill_buffer_time);
  }

  if (!underflow_cb.is_null())
    underflow_cb.Run();

  return frames_written;
}

void AudioRendererBase::SignalEndOfStream() {
  DCHECK(received_end_of_stream_);
  if (!rendered_end_of_stream_) {
    rendered_end_of_stream_ = true;
    host()->NotifyEnded();
  }
}

void AudioRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  if (pending_read_ || state_ == kPaused)
    return;
  pending_read_ = true;
  decoder_->Read(read_cb_);
}

void AudioRendererBase::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  algorithm_->SetPlaybackRate(playback_rate);
}

float AudioRendererBase::GetPlaybackRate() {
  base::AutoLock auto_lock(lock_);
  return algorithm_->playback_rate();
}

bool AudioRendererBase::IsBeforeSeekTime(const scoped_refptr<Buffer>& buffer) {
  return (state_ == kSeeking) && buffer && !buffer->IsEndOfStream() &&
      (buffer->GetTimestamp() + buffer->GetDuration()) < seek_timestamp_;
}

}  // namespace media
