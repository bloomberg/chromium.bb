// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_renderer_base.h"

#include <algorithm>
#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "media/base/filter_host.h"
#include "media/filters/audio_renderer_algorithm_ola.h"

namespace media {

AudioRendererBase::AudioRendererBase()
    : state_(kUninitialized),
      recieved_end_of_stream_(false),
      rendered_end_of_stream_(false),
      pending_reads_(0) {
}

AudioRendererBase::~AudioRendererBase() {
  // Stop() should have been called and |algorithm_| should have been destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!algorithm_.get());
}

void AudioRendererBase::Play(FilterCallback* callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  scoped_ptr<FilterCallback> c(callback);
  state_ = kPlaying;
  callback->Run();
}

void AudioRendererBase::Pause(FilterCallback* callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPlaying, state_);
  pause_callback_.reset(callback);
  state_ = kPaused;

  // We'll only pause when we've finished all pending reads.
  if (pending_reads_ == 0) {
    pause_callback_->Run();
    pause_callback_.reset();
  } else {
    state_ = kPaused;
  }
}

void AudioRendererBase::Stop(FilterCallback* callback) {
  OnStop();
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;
    algorithm_.reset(NULL);
  }
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void AudioRendererBase::Seek(base::TimeDelta time, FilterCallback* callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
  state_ = kSeeking;
  seek_callback_.reset(callback);
  seek_timestamp_ = time;

  // Throw away everything and schedule our reads.
  last_fill_buffer_time_ = base::TimeDelta();
  recieved_end_of_stream_ = false;
  rendered_end_of_stream_ = false;

  // |algorithm_| will request more reads.
  algorithm_->FlushBuffers();
}

void AudioRendererBase::Initialize(AudioDecoder* decoder,
                                   FilterCallback* callback) {
  DCHECK(decoder);
  DCHECK(callback);
  DCHECK_EQ(kUninitialized, state_);
  scoped_ptr<FilterCallback> c(callback);
  decoder_ = decoder;

  decoder_->set_consume_audio_samples_callback(
      NewCallback(this, &AudioRendererBase::ConsumeAudioSamples));
  // Get the media properties to initialize our algorithms.
  int channels = 0;
  int sample_rate = 0;
  int sample_bits = 0;
  if (!ParseMediaFormat(decoder_->media_format(), &channels, &sample_rate,
                        &sample_bits)) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback->Run();
    return;
  }

  // Create a callback so our algorithm can request more reads.
  AudioRendererAlgorithmBase::RequestReadCallback* cb =
      NewCallback(this, &AudioRendererBase::ScheduleRead_Locked);

  // Construct the algorithm.
  algorithm_.reset(new AudioRendererAlgorithmOLA());

  // Initialize our algorithm with media properties, initial playback rate,
  // and a callback to request more reads from the data source.
  algorithm_->Initialize(channels,
                         sample_rate,
                         sample_bits,
                         0.0f,
                         cb);

  // Give the subclass an opportunity to initialize itself.
  if (!OnInitialize(decoder_->media_format())) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback->Run();
    return;
  }

  // Finally, execute the start callback.
  state_ = kPaused;
  callback->Run();
}

bool AudioRendererBase::HasEnded() {
  base::AutoLock auto_lock(lock_);
  if (rendered_end_of_stream_) {
    DCHECK(algorithm_->IsQueueEmpty())
        << "Audio queue should be empty if we have rendered end of stream";
  }
  return recieved_end_of_stream_ && rendered_end_of_stream_;
}

void AudioRendererBase::ConsumeAudioSamples(scoped_refptr<Buffer> buffer_in) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying);
  DCHECK_GT(pending_reads_, 0u);
  --pending_reads_;

  // TODO(scherkus): this happens due to a race, primarily because Stop() is a
  // synchronous call when it should be asynchronous and accept a callback.
  // Refer to http://crbug.com/16059
  if (state_ == kStopped) {
    return;
  }

  // Don't enqueue an end-of-stream buffer because it has no data, otherwise
  // discard decoded audio data until we reach our desired seek timestamp.
  if (buffer_in->IsEndOfStream()) {
    recieved_end_of_stream_ = true;
  } else if (state_ == kSeeking && !buffer_in->IsEndOfStream() &&
             (buffer_in->GetTimestamp() + buffer_in->GetDuration()) <
                 seek_timestamp_) {
    ScheduleRead_Locked();
  } else {
    // Note: Calling this may schedule more reads.
    algorithm_->EnqueueBuffer(buffer_in);
  }

  // Check for our preroll complete condition.
  if (state_ == kSeeking) {
    DCHECK(seek_callback_.get());
    if (algorithm_->IsQueueFull() || recieved_end_of_stream_) {
      // Transition into paused whether we have data in |algorithm_| or not.
      // FillBuffer() will play silence if there's nothing to fill.
      state_ = kPaused;
      seek_callback_->Run();
      seek_callback_.reset();
    }
  } else if (state_ == kPaused && pending_reads_ == 0) {
    // No more pending reads!  We're now officially "paused".
    if (pause_callback_.get()) {
      pause_callback_->Run();
      pause_callback_.reset();
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
  {
    base::AutoLock auto_lock(lock_);

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      // TODO(scherkus): To keep the audio hardware busy we write at most 8k of
      // zeros.  This gets around the tricky situation of pausing and resuming
      // the audio IPC layer in Chrome.  Ideally, we should return zero and then
      // the subclass can restart the conversation.
      const uint32 kZeroLength = 8192;
      dest_written = std::min(kZeroLength, dest_len);
      memset(dest, 0, dest_written);
      return dest_written;
    }

    // Save a local copy of last fill buffer time and reset the member.
    last_fill_buffer_time = last_fill_buffer_time_;
    last_fill_buffer_time_ = base::TimeDelta();

    // Use two conditions to determine the end of playback:
    // 1. Algorithm has no audio data.
    // 2. Browser process has no audio data.
    if (algorithm_->IsQueueEmpty() && buffers_empty) {
      if (recieved_end_of_stream_ && !rendered_end_of_stream_) {
        rendered_end_of_stream_ = true;
        host()->NotifyEnded();
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

  return dest_written;
}

void AudioRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  ++pending_reads_;
  // TODO(jiesun): We use dummy buffer to feed decoder to let decoder to
  // provide buffer pools. In the future, we may want to implement real
  // buffer pool to recycle buffers.
  scoped_refptr<Buffer> buffer;
  decoder_->ProduceAudioSamples(buffer);
}

// static
bool AudioRendererBase::ParseMediaFormat(const MediaFormat& media_format,
                                         int* channels_out,
                                         int* sample_rate_out,
                                         int* sample_bits_out) {
  // TODO(scherkus): might be handy to support NULL parameters.
  return media_format.GetAsInteger(MediaFormat::kChannels, channels_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleRate, sample_rate_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleBits, sample_bits_out);
}

void AudioRendererBase::SetPlaybackRate(float playback_rate) {
  algorithm_->set_playback_rate(playback_rate);
}

float AudioRendererBase::GetPlaybackRate() {
  return algorithm_->playback_rate();
}

}  // namespace media
