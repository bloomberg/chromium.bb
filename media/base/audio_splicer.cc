// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_splicer.h"

#include <cstdlib>
#include <deque>

#include "base/logging.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/vector_math.h"

namespace media {

// Largest gap or overlap allowed by this class. Anything
// larger than this will trigger an error.
// This is an arbitrary value, but the initial selection of 50ms
// roughly represents the duration of 2 compressed AAC or MP3 frames.
static const int kMaxTimeDeltaInMilliseconds = 50;

// Minimum gap size needed before the splicer will take action to
// fill a gap. This avoids periodically inserting and then dropping samples
// when the buffer timestamps are slightly off because of timestamp rounding
// in the source content. Unit is frames.
static const int kMinGapSize = 2;

// AudioBuffer::TrimStart() is not as accurate as the timestamp helper, so
// manually adjust the duration and timestamp after trimming.
static void AccurateTrimStart(int frames_to_trim,
                              const scoped_refptr<AudioBuffer> buffer,
                              const AudioTimestampHelper& timestamp_helper) {
  buffer->TrimStart(frames_to_trim);
  buffer->set_timestamp(timestamp_helper.GetTimestamp());
  buffer->set_duration(
      timestamp_helper.GetFrameDuration(buffer->frame_count()));
}

// AudioBuffer::TrimEnd() is not as accurate as the timestamp helper, so
// manually adjust the duration after trimming.
static void AccurateTrimEnd(int frames_to_trim,
                            const scoped_refptr<AudioBuffer> buffer,
                            const AudioTimestampHelper& timestamp_helper) {
  DCHECK_LT(std::abs(timestamp_helper.GetFramesToTarget(buffer->timestamp())),
            kMinGapSize);
  buffer->TrimEnd(frames_to_trim);
  buffer->set_duration(
      timestamp_helper.GetFrameDuration(buffer->frame_count()));
}

// Returns an AudioBus whose frame buffer is backed by the provided AudioBuffer.
static scoped_ptr<AudioBus> CreateAudioBufferWrapper(
    const scoped_refptr<AudioBuffer>& buffer) {
  scoped_ptr<AudioBus> wrapper =
      AudioBus::CreateWrapper(buffer->channel_count());
  wrapper->set_frames(buffer->frame_count());
  for (int ch = 0; ch < buffer->channel_count(); ++ch) {
    wrapper->SetChannelData(
        ch, reinterpret_cast<float*>(buffer->channel_data()[ch]));
  }
  return wrapper.Pass();
}

class AudioStreamSanitizer {
 public:
  explicit AudioStreamSanitizer(int samples_per_second);
  ~AudioStreamSanitizer();

  // Resets the sanitizer state by clearing the output buffers queue, and
  // resetting the timestamp helper.
  void Reset();

  // Similar to Reset(), but initializes the timestamp helper with the given
  // parameters.
  void ResetTimestampState(int64 frame_count, base::TimeDelta base_timestamp);

  // Adds a new buffer full of samples or end of stream buffer to the splicer.
  // Returns true if the buffer was accepted. False is returned if an error
  // occurred.
  bool AddInput(const scoped_refptr<AudioBuffer>& input);

  // Returns true if the sanitizer has a buffer to return.
  bool HasNextBuffer() const;

  // Removes the next buffer from the output buffer queue and returns it; should
  // only be called if HasNextBuffer() returns true.
  scoped_refptr<AudioBuffer> GetNextBuffer();

  // Returns the total frame count of all buffers available for output.
  int GetFrameCount() const;

  // Returns the duration of all buffers added to the output queue thus far.
  base::TimeDelta GetDuration() const;

  const AudioTimestampHelper& timestamp_helper() {
    return output_timestamp_helper_;
  }

  // Transfer all buffers into |output|.  Returns false if AddInput() on the
  // |output| sanitizer fails for any buffer removed from |this|.
  bool DrainInto(AudioStreamSanitizer* output);

 private:
  void AddOutputBuffer(const scoped_refptr<AudioBuffer>& buffer);

  AudioTimestampHelper output_timestamp_helper_;
  bool received_end_of_stream_;

  typedef std::deque<scoped_refptr<AudioBuffer> > BufferQueue;
  BufferQueue output_buffers_;

  DISALLOW_ASSIGN(AudioStreamSanitizer);
};

AudioStreamSanitizer::AudioStreamSanitizer(int samples_per_second)
    : output_timestamp_helper_(samples_per_second),
      received_end_of_stream_(false) {}

AudioStreamSanitizer::~AudioStreamSanitizer() {}

void AudioStreamSanitizer::Reset() {
  ResetTimestampState(0, kNoTimestamp());
}

void AudioStreamSanitizer::ResetTimestampState(int64 frame_count,
                                               base::TimeDelta base_timestamp) {
  output_buffers_.clear();
  received_end_of_stream_ = false;
  output_timestamp_helper_.SetBaseTimestamp(base_timestamp);
  if (frame_count > 0)
    output_timestamp_helper_.AddFrames(frame_count);
}

bool AudioStreamSanitizer::AddInput(const scoped_refptr<AudioBuffer>& input) {
  DCHECK(!received_end_of_stream_ || input->end_of_stream());

  if (input->end_of_stream()) {
    output_buffers_.push_back(input);
    received_end_of_stream_ = true;
    return true;
  }

  DCHECK(input->timestamp() != kNoTimestamp());
  DCHECK(input->duration() > base::TimeDelta());
  DCHECK_GT(input->frame_count(), 0);

  if (output_timestamp_helper_.base_timestamp() == kNoTimestamp())
    output_timestamp_helper_.SetBaseTimestamp(input->timestamp());

  if (output_timestamp_helper_.base_timestamp() > input->timestamp()) {
    DVLOG(1) << "Input timestamp is before the base timestamp.";
    return false;
  }

  const base::TimeDelta timestamp = input->timestamp();
  const base::TimeDelta expected_timestamp =
      output_timestamp_helper_.GetTimestamp();
  const base::TimeDelta delta = timestamp - expected_timestamp;

  if (std::abs(delta.InMilliseconds()) > kMaxTimeDeltaInMilliseconds) {
    DVLOG(1) << "Timestamp delta too large: " << delta.InMicroseconds() << "us";
    return false;
  }

  int frames_to_fill = 0;
  if (delta != base::TimeDelta())
    frames_to_fill = output_timestamp_helper_.GetFramesToTarget(timestamp);

  if (frames_to_fill == 0 || std::abs(frames_to_fill) < kMinGapSize) {
    AddOutputBuffer(input);
    return true;
  }

  if (frames_to_fill > 0) {
    DVLOG(1) << "Gap detected @ " << expected_timestamp.InMicroseconds()
             << " us: " << delta.InMicroseconds() << " us";

    // Create a buffer with enough silence samples to fill the gap and
    // add it to the output buffer.
    scoped_refptr<AudioBuffer> gap = AudioBuffer::CreateEmptyBuffer(
        input->channel_layout(),
        input->channel_count(),
        input->sample_rate(),
        frames_to_fill,
        expected_timestamp,
        output_timestamp_helper_.GetFrameDuration(frames_to_fill));
    AddOutputBuffer(gap);

    // Add the input buffer now that the gap has been filled.
    AddOutputBuffer(input);
    return true;
  }

  // Overlapping buffers marked as splice frames are handled by AudioSplicer,
  // but decoder and demuxer quirks may sometimes produce overlapping samples
  // which need to be sanitized.
  //
  // A crossfade can't be done here because only the current buffer is available
  // at this point, not previous buffers.
  DVLOG(1) << "Overlap detected @ " << expected_timestamp.InMicroseconds()
           << " us: " << -delta.InMicroseconds() << " us";

  const int frames_to_skip = -frames_to_fill;
  if (input->frame_count() <= frames_to_skip) {
    DVLOG(1) << "Dropping whole buffer";
    return true;
  }

  // Copy the trailing samples that do not overlap samples already output
  // into a new buffer.  Add this new buffer to the output queue.
  //
  // TODO(acolwell): Implement a cross-fade here so the transition is less
  // jarring.
  AccurateTrimStart(frames_to_skip, input, output_timestamp_helper_);
  AddOutputBuffer(input);
  return true;
}

bool AudioStreamSanitizer::HasNextBuffer() const {
  return !output_buffers_.empty();
}

scoped_refptr<AudioBuffer> AudioStreamSanitizer::GetNextBuffer() {
  scoped_refptr<AudioBuffer> ret = output_buffers_.front();
  output_buffers_.pop_front();
  return ret;
}

void AudioStreamSanitizer::AddOutputBuffer(
    const scoped_refptr<AudioBuffer>& buffer) {
  output_timestamp_helper_.AddFrames(buffer->frame_count());
  output_buffers_.push_back(buffer);
}

int AudioStreamSanitizer::GetFrameCount() const {
  int frame_count = 0;
  for (BufferQueue::const_iterator it = output_buffers_.begin();
       it != output_buffers_.end(); ++it) {
    frame_count += (*it)->frame_count();
  }
  return frame_count;
}

base::TimeDelta AudioStreamSanitizer::GetDuration() const {
  DCHECK(output_timestamp_helper_.base_timestamp() != kNoTimestamp());
  return output_timestamp_helper_.GetTimestamp() -
         output_timestamp_helper_.base_timestamp();
}

bool AudioStreamSanitizer::DrainInto(AudioStreamSanitizer* output) {
  while (HasNextBuffer()) {
    if (!output->AddInput(GetNextBuffer()))
      return false;
  }
  return true;
}

AudioSplicer::AudioSplicer(int samples_per_second)
    : max_crossfade_duration_(
          base::TimeDelta::FromMilliseconds(kCrossfadeDurationInMilliseconds)),
      splice_timestamp_(kNoTimestamp()),
      max_splice_end_timestamp_(kNoTimestamp()),
      output_sanitizer_(new AudioStreamSanitizer(samples_per_second)),
      pre_splice_sanitizer_(new AudioStreamSanitizer(samples_per_second)),
      post_splice_sanitizer_(new AudioStreamSanitizer(samples_per_second)) {}

AudioSplicer::~AudioSplicer() {}

void AudioSplicer::Reset() {
  output_sanitizer_->Reset();
  pre_splice_sanitizer_->Reset();
  post_splice_sanitizer_->Reset();
  reset_splice_timestamps();
}

bool AudioSplicer::AddInput(const scoped_refptr<AudioBuffer>& input) {
  // If we're not processing a splice, add the input to the output queue.
  if (splice_timestamp_ == kNoTimestamp()) {
    DCHECK(!pre_splice_sanitizer_->HasNextBuffer());
    DCHECK(!post_splice_sanitizer_->HasNextBuffer());
    return output_sanitizer_->AddInput(input);
  }

  const AudioTimestampHelper& output_ts_helper =
      output_sanitizer_->timestamp_helper();

  if (!post_splice_sanitizer_->HasNextBuffer()) {
    // Due to inaccurate demuxer timestamps a splice may have been incorrectly
    // marked such that the "pre splice buffers" were all entirely before the
    // splice point.  Here we check for end of stream or buffers which couldn't
    // possibly be part of the splice.
    if (input->end_of_stream() ||
        input->timestamp() >= max_splice_end_timestamp_) {
      CHECK(pre_splice_sanitizer_->DrainInto(output_sanitizer_.get()));
      reset_splice_timestamps();
      return output_sanitizer_->AddInput(input);
    }

    // If the provided buffer is entirely before the splice point it can also be
    // added to the output queue.
    if (input->timestamp() + input->duration() < splice_timestamp_) {
      DCHECK(!pre_splice_sanitizer_->HasNextBuffer());
      return output_sanitizer_->AddInput(input);
    }

    // If we've encountered the first pre splice buffer, reset the pre splice
    // sanitizer based on |output_sanitizer_|.  This is done so that gaps and
    // overlaps between buffers across the sanitizers are accounted for prior
    // to calculating crossfade.
    if (!pre_splice_sanitizer_->HasNextBuffer()) {
      pre_splice_sanitizer_->ResetTimestampState(
          output_ts_helper.frame_count(), output_ts_helper.base_timestamp());
    }

    // The first overlapping buffer is expected to have a timestamp of exactly
    // |splice_timestamp_|.  Until that timestamp is seen, all buffers go into
    // |pre_splice_sanitizer_|.
    if (!pre_splice_sanitizer_->HasNextBuffer() ||
        input->timestamp() != splice_timestamp_) {
      return pre_splice_sanitizer_->AddInput(input);
    }

    // We've received the first overlapping buffer.
  } else if (!input->end_of_stream() &&
             input->timestamp() == splice_timestamp_) {
    // In this case we accidentally put a pre splice buffer in the post splice
    // sanitizer, so we need to recover by transferring all current post splice
    // buffers into the pre splice sanitizer and continuing with the splice.
    post_splice_sanitizer_->DrainInto(pre_splice_sanitizer_.get());
    post_splice_sanitizer_->Reset();
  }

  // At this point we have all the fade out preroll buffers from the decoder.
  // We now need to wait until we have enough data to perform the crossfade (or
  // we receive an end of stream).
  if (!post_splice_sanitizer_->AddInput(input))
    return false;

  // Ensure |output_sanitizer_| has a valid base timestamp so we can use it for
  // timestamp calculations.
  if (output_ts_helper.base_timestamp() == kNoTimestamp()) {
    output_sanitizer_->ResetTimestampState(
        0, pre_splice_sanitizer_->timestamp_helper().base_timestamp());
  }

  // If a splice frame was incorrectly marked due to poor demuxed timestamps, we
  // may not actually have a splice.  Here we check if any frames exist before
  // the splice.  In this case, just transfer all data to the output sanitizer.
  if (pre_splice_sanitizer_->GetFrameCount() <=
      output_ts_helper.GetFramesToTarget(splice_timestamp_)) {
    CHECK(pre_splice_sanitizer_->DrainInto(output_sanitizer_.get()));
    CHECK(post_splice_sanitizer_->DrainInto(output_sanitizer_.get()));
    reset_splice_timestamps();
    return true;
  }

  // Since it's possible that a pre splice buffer after the first might have its
  // timestamp fuzzed to be |splice_timestamp_| we need to always wait until we
  // have seen all possible post splice buffers to ensure we didn't mistakenly
  // put a pre splice buffer into the post splice sanitizer.
  if (!input->end_of_stream() && input->timestamp() < max_splice_end_timestamp_)
    return true;

  scoped_refptr<AudioBuffer> crossfade_buffer;
  scoped_ptr<AudioBus> pre_splice =
      ExtractCrossfadeFromPreSplice(&crossfade_buffer);

  // Crossfade the pre splice and post splice sections and transfer all relevant
  // buffers into |output_sanitizer_|.
  CrossfadePostSplice(pre_splice.Pass(), crossfade_buffer);

  // Clear the splice timestamp so new splices can be accepted.
  reset_splice_timestamps();
  return true;
}

bool AudioSplicer::HasNextBuffer() const {
  return output_sanitizer_->HasNextBuffer();
}

scoped_refptr<AudioBuffer> AudioSplicer::GetNextBuffer() {
  return output_sanitizer_->GetNextBuffer();
}

void AudioSplicer::SetSpliceTimestamp(base::TimeDelta splice_timestamp) {
  DCHECK(splice_timestamp != kNoTimestamp());
  if (splice_timestamp_ == splice_timestamp)
    return;

  // TODO(dalecurtis): We may need the concept of a future_splice_timestamp_ to
  // handle cases where another splice comes in before we've received 5ms of
  // data from the last one.  Leave this as a CHECK for now to figure out if
  // this case is possible.
  CHECK(splice_timestamp_ == kNoTimestamp());
  splice_timestamp_ = splice_timestamp;
  max_splice_end_timestamp_ = splice_timestamp_ + max_crossfade_duration_;
  pre_splice_sanitizer_->Reset();
  post_splice_sanitizer_->Reset();
}

scoped_ptr<AudioBus> AudioSplicer::ExtractCrossfadeFromPreSplice(
    scoped_refptr<AudioBuffer>* crossfade_buffer) {
  DCHECK(crossfade_buffer);
  const AudioTimestampHelper& output_ts_helper =
      output_sanitizer_->timestamp_helper();

  int frames_before_splice =
      output_ts_helper.GetFramesToTarget(splice_timestamp_);

  // Determine crossfade frame count based on available frames in each splicer
  // and capping to the maximum crossfade duration.
  const int max_crossfade_frame_count =
      output_ts_helper.GetFramesToTarget(max_splice_end_timestamp_) -
      frames_before_splice;
  const int frames_to_crossfade = std::min(
      max_crossfade_frame_count,
      std::min(pre_splice_sanitizer_->GetFrameCount() - frames_before_splice,
               post_splice_sanitizer_->GetFrameCount()));
  // There must always be frames to crossfade, otherwise the splice should not
  // have been generated.
  DCHECK_GT(frames_to_crossfade, 0);

  int frames_read = 0;
  scoped_ptr<AudioBus> output_bus;
  while (pre_splice_sanitizer_->HasNextBuffer() &&
         frames_read < frames_to_crossfade) {
    scoped_refptr<AudioBuffer> preroll = pre_splice_sanitizer_->GetNextBuffer();

    // We don't know the channel count until we see the first buffer, so wait
    // until the first buffer to allocate the output AudioBus.
    if (!output_bus) {
      output_bus =
          AudioBus::Create(preroll->channel_count(), frames_to_crossfade);
      // Allocate output buffer for crossfade.
      *crossfade_buffer = AudioBuffer::CreateBuffer(kSampleFormatPlanarF32,
                                                    preroll->channel_layout(),
                                                    preroll->channel_count(),
                                                    preroll->sample_rate(),
                                                    frames_to_crossfade);
    }

    // There may be enough of a gap introduced during decoding such that an
    // entire buffer exists before the splice point.
    if (frames_before_splice >= preroll->frame_count()) {
      // Adjust the number of frames remaining before the splice.  NOTE: This is
      // safe since |pre_splice_sanitizer_| is a continuation of the timeline in
      // |output_sanitizer_|.  As such we're guaranteed there are no gaps or
      // overlaps in the timeline between the two sanitizers.
      frames_before_splice -= preroll->frame_count();
      CHECK(output_sanitizer_->AddInput(preroll));
      continue;
    }

    const int frames_to_read =
        std::min(preroll->frame_count() - frames_before_splice,
                 output_bus->frames() - frames_read);
    preroll->ReadFrames(
        frames_to_read, frames_before_splice, frames_read, output_bus.get());
    frames_read += frames_to_read;

    // If only part of the buffer was consumed, trim it appropriately and stick
    // it into the output queue.
    if (frames_before_splice) {
      AccurateTrimEnd(preroll->frame_count() - frames_before_splice,
                      preroll,
                      output_ts_helper);
      CHECK(output_sanitizer_->AddInput(preroll));
      frames_before_splice = 0;
    }
  }

  // All necessary buffers have been processed, it's safe to reset.
  pre_splice_sanitizer_->Reset();
  DCHECK_EQ(output_bus->frames(), frames_read);
  DCHECK_EQ(output_ts_helper.GetFramesToTarget(splice_timestamp_), 0);
  return output_bus.Pass();
}

void AudioSplicer::CrossfadePostSplice(
    scoped_ptr<AudioBus> pre_splice_bus,
    scoped_refptr<AudioBuffer> crossfade_buffer) {
  // Use the calculated timestamp and duration to ensure there's no extra gaps
  // or overlaps to process when adding the buffer to |output_sanitizer_|.
  const AudioTimestampHelper& output_ts_helper =
      output_sanitizer_->timestamp_helper();
  crossfade_buffer->set_timestamp(output_ts_helper.GetTimestamp());
  crossfade_buffer->set_duration(
      output_ts_helper.GetFrameDuration(pre_splice_bus->frames()));

  // AudioBuffer::ReadFrames() only allows output into an AudioBus, so wrap
  // our AudioBuffer in one so we can avoid extra data copies.
  scoped_ptr<AudioBus> output_bus = CreateAudioBufferWrapper(crossfade_buffer);

  // Extract crossfade section from the |post_splice_sanitizer_|.
  int frames_read = 0, frames_to_trim = 0;
  scoped_refptr<AudioBuffer> remainder;
  while (post_splice_sanitizer_->HasNextBuffer() &&
         frames_read < output_bus->frames()) {
    scoped_refptr<AudioBuffer> postroll =
        post_splice_sanitizer_->GetNextBuffer();
    const int frames_to_read =
        std::min(postroll->frame_count(), output_bus->frames() - frames_read);
    postroll->ReadFrames(frames_to_read, 0, frames_read, output_bus.get());
    frames_read += frames_to_read;

    // If only part of the buffer was consumed, save it for after we've added
    // the crossfade buffer
    if (frames_to_read < postroll->frame_count()) {
      DCHECK(!remainder);
      remainder.swap(postroll);
      frames_to_trim = frames_to_read;
    }
  }

  DCHECK_EQ(output_bus->frames(), frames_read);

  // Crossfade the audio into |crossfade_buffer|.
  for (int ch = 0; ch < output_bus->channels(); ++ch) {
    vector_math::Crossfade(pre_splice_bus->channel(ch),
                           pre_splice_bus->frames(),
                           output_bus->channel(ch));
  }

  CHECK(output_sanitizer_->AddInput(crossfade_buffer));
  DCHECK_EQ(crossfade_buffer->frame_count(), output_bus->frames());

  if (remainder) {
    // Trim off consumed frames.
    AccurateTrimStart(frames_to_trim, remainder, output_ts_helper);
    CHECK(output_sanitizer_->AddInput(remainder));
  }

  // Transfer all remaining buffers out and reset once empty.
  CHECK(post_splice_sanitizer_->DrainInto(output_sanitizer_.get()));
  post_splice_sanitizer_->Reset();
}

}  // namespace media
