// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "media/base/filter_host.h"
#include "media/filters/audio_renderer_base.h"

namespace media {

// The maximum size of the queue, which also acts as the number of initial reads
// to perform for buffering.  The size of the queue should never exceed this
// number since we read only after we've dequeued and released a buffer in
// callback thread.
//
// This is sort of a magic number, but for 44.1kHz stereo audio this will give
// us enough data to fill approximately 4 complete callback buffers.
const size_t AudioRendererBase::kDefaultMaxQueueSize = 16;

AudioRendererBase::AudioRendererBase(size_t max_queue_size)
    : max_queue_size_(max_queue_size),
      data_offset_(0),
      state_(kUninitialized),
      pending_reads_(0) {
}

AudioRendererBase::~AudioRendererBase() {
  // Stop() should have been called and OnReadComplete() should have stopped
  // enqueuing data.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(queue_.empty());
}

void AudioRendererBase::Play(FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  scoped_ptr<FilterCallback> c(callback);
  state_ = kPlaying;
  callback->Run();
}

void AudioRendererBase::Pause(FilterCallback* callback) {
  AutoLock auto_lock(lock_);
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

void AudioRendererBase::Stop() {
  OnStop();

  AutoLock auto_lock(lock_);
  state_ = kStopped;
  queue_.clear();
}

void AudioRendererBase::Seek(base::TimeDelta time, FilterCallback* callback) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(kPaused, state_);
  DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
  state_ = kSeeking;
  seek_callback_.reset(callback);

  // Throw away everything and schedule our reads.
  last_fill_buffer_time_ = base::TimeDelta();
  queue_.clear();
  data_offset_ = 0;
  for (size_t i = 0; i < max_queue_size_; ++i) {
    ScheduleRead_Locked();
  }
}

void AudioRendererBase::Initialize(AudioDecoder* decoder,
                                   FilterCallback* callback) {
  DCHECK(decoder);
  DCHECK(callback);
  DCHECK_EQ(kUninitialized, state_);
  scoped_ptr<FilterCallback> c(callback);
  decoder_ = decoder;

  // Defer initialization until all scheduled reads have completed.
  if (!OnInitialize(decoder_->media_format())) {
    host()->SetError(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback->Run();
    return;
  }

  // Finally, execute the start callback.
  state_ = kPaused;
  callback->Run();
}

void AudioRendererBase::OnReadComplete(Buffer* buffer_in) {
  AutoLock auto_lock(lock_);
  DCHECK(state_ == kPaused || state_ == kSeeking || state_ == kPlaying);
  DCHECK_GT(pending_reads_, 0u);
  --pending_reads_;

  // If we have stopped don't enqueue, same for end of stream buffer since
  // it has no data.
  if (!buffer_in->IsEndOfStream()) {
    queue_.push_back(buffer_in);
    DCHECK(queue_.size() <= max_queue_size_);
  }

  // Check for our preroll complete condition.
  if (state_ == kSeeking) {
    DCHECK(seek_callback_.get());
    if (queue_.size() == max_queue_size_ || buffer_in->IsEndOfStream()) {
      // Transition into paused whether we have data in |queue_| or not.
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

// TODO(scherkus): clean up FillBuffer().. it's overly complex!!
size_t AudioRendererBase::FillBuffer(uint8* dest,
                                     size_t dest_len,
                                     float rate,
                                     const base::TimeDelta& playback_delay) {
  size_t dest_written = 0;

  // The timestamp of the last buffer written during the last call to
  // FillBuffer().
  base::TimeDelta last_fill_buffer_time;
  {
    AutoLock auto_lock(lock_);

    // Mute audio by returning 0 when not playing.
    if (state_ != kPlaying) {
      // TODO(scherkus): To keep the audio hardware busy we write at most 8k of
      // zeros.  This gets around the tricky situation of pausing and resuming
      // the audio IPC layer in Chrome.  Ideally, we should return zero and then
      // the subclass can restart the conversation.
      const size_t kZeroLength = 8192;
      dest_written = std::min(kZeroLength, dest_len);
      memset(dest, 0, dest_written);
      return dest_written;
    }

    // Save a local copy of last fill buffer time and reset the member.
    last_fill_buffer_time = last_fill_buffer_time_;
    last_fill_buffer_time_ = base::TimeDelta();

    // Loop until the buffer has been filled.
    while (dest_len > 0 && !queue_.empty()) {
      scoped_refptr<Buffer> buffer = queue_.front();

      // Determine how much to copy.
      DCHECK_LE(data_offset_, buffer->GetDataSize());
      const uint8* data = buffer->GetData() + data_offset_;
      size_t data_len = buffer->GetDataSize() - data_offset_;

      // New scaled packet size aligned to 16 to ensure it's on a
      // channel/sample boundary.  Only guaranteed to work for power of 2
      // number of channels and sample size.
      size_t scaled_data_len = (rate <= 0.0f) ? 0 :
          static_cast<size_t>(data_len / rate) & ~15;
      if (scaled_data_len > dest_len) {
        data_len = (data_len * dest_len / scaled_data_len) & ~15;
        scaled_data_len = dest_len;
      }

      // Handle playback rate in three different cases:
      // 1. If rate >= 1.0
      //    Speed up the playback, we copy partial amount of decoded samples
      //    into target buffer.
      // 2. If 0.5 <= rate < 1.0
      //    Slow down the playback, duplicate the decoded samples to fill a
      //    larger size of target buffer.
      // 3. If rate < 0.5
      //    Playback is too slow, simply mute the audio.
      // TODO(hclam): the logic for handling playback rate is too complex and
      // is not careful enough. I should do some bounds checking and even better
      // replace this with a better/clearer implementation.
      if (rate >= 1.0f) {
        memcpy(dest, data, scaled_data_len);
      } else if (rate >= 0.5) {
        memcpy(dest, data, data_len);
        memcpy(dest + data_len, data, scaled_data_len - data_len);
      } else {
        memset(dest, 0, data_len);
      }
      dest += scaled_data_len;
      dest_len -= scaled_data_len;
      dest_written += scaled_data_len;

      data_offset_ += data_len;

      if (rate == 0.0f) {
        dest_written = 0;
        break;
      }

      // Check to see if we're finished with the front buffer.
      if (buffer->GetDataSize() - data_offset_ < 16) {
        // Update the time.  If this is the last buffer in the queue, we'll
        // drop out of the loop before len == 0, so we need to always update
        // the time here.
        if (buffer->GetTimestamp().InMicroseconds() > 0) {
          last_fill_buffer_time_ = buffer->GetTimestamp() +
                                   buffer->GetDuration();
        }

        // Dequeue the buffer and request another.
        queue_.pop_front();
        ScheduleRead_Locked();

        // Reset our offset into the front buffer.
        data_offset_ = 0;
      } else {
        // If we're done with the read, compute the time.
        // Integer divide so multiply before divide to work properly.
        int64 us_written = (buffer->GetDuration().InMicroseconds() *
                            data_offset_) / buffer->GetDataSize();

        if (buffer->GetTimestamp().InMicroseconds() > 0) {
          last_fill_buffer_time_ =
              buffer->GetTimestamp() +
              base::TimeDelta::FromMicroseconds(us_written);
        }
      }
    }
  }

  // Update the pipeline's time if it was set last time.
  if (last_fill_buffer_time.InMicroseconds() > 0) {
    // Adjust the |last_fill_buffer_time| with the playback delay.
    // TODO(hclam): If there is a playback delay, the pipeline would not be
    // updated with a correct timestamp when the stream is played at the very
    // end since we use decoded packets to trigger time updates. A better
    // solution is to start a timer when an audio packet is decoded to allow
    // finer time update events.
    if (playback_delay < last_fill_buffer_time)
      last_fill_buffer_time -= playback_delay;
    host()->SetTime(last_fill_buffer_time);
  }

  return dest_written;
}

void AudioRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  DCHECK_LT(pending_reads_, max_queue_size_);
  ++pending_reads_;
  decoder_->Read(NewCallback(this, &AudioRendererBase::OnReadComplete));
}

// static
bool AudioRendererBase::ParseMediaFormat(const MediaFormat& media_format,
                                         int* channels_out,
                                         int* sample_rate_out,
                                         int* sample_bits_out) {
  // TODO(scherkus): might be handy to support NULL parameters.
  std::string mime_type;
  return media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      media_format.GetAsInteger(MediaFormat::kChannels, channels_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleRate, sample_rate_out) &&
      media_format.GetAsInteger(MediaFormat::kSampleBits, sample_bits_out) &&
      mime_type.compare(mime_type::kUncompressedAudio) == 0;
}

}  // namespace media
