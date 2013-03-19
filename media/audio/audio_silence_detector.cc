// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_silence_detector.h"

#include "base/float_util.h"
#include "base/time.h"
#include "media/base/audio_bus.h"

using base::AtomicRefCountDec;
using base::AtomicRefCountInc;
using base::AtomicRefCountIsOne;
using base::AtomicRefCountIsZero;

namespace media {

AudioSilenceDetector::AudioSilenceDetector(
    int sample_rate,
    const base::TimeDelta& questionable_silence_period,
    float indistinguishable_silence_threshold)
    : polling_period_(questionable_silence_period),
      frames_before_observing_silence_(
          sample_rate * questionable_silence_period.InSecondsF()),
      silence_threshold_(indistinguishable_silence_threshold),
      frames_silent_so_far_(frames_before_observing_silence_),
      observing_silence_(1),
      was_audible_(false) {
}

AudioSilenceDetector::~AudioSilenceDetector() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Note: If active, ~RepeatingTimer() will StopAndAbandon().
}

void AudioSilenceDetector::Start(const AudibleCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(notify_is_audible_.is_null());
  DCHECK(!callback.is_null());

  notify_is_audible_ = callback;
  was_audible_ = AtomicRefCountIsZero(&observing_silence_);
  notify_is_audible_.Run(was_audible_);
  poll_timer_.Start(
      FROM_HERE, polling_period_,
      this, &AudioSilenceDetector::MaybeInvokeAudibleCallback);
}

void AudioSilenceDetector::Stop(bool notify_ending_in_silence) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!notify_is_audible_.is_null());

  poll_timer_.Stop();
  if (notify_ending_in_silence)
    notify_is_audible_.Run(false);
  notify_is_audible_.Reset();
}

void AudioSilenceDetector::Scan(const AudioBus* buffer, int frames) {
  // Determine whether the frames just read are probably silence.  If enough
  // frames of silence have been observed, flip the |observing_silence_|
  // boolean, which will be read by another thread.
  if (ProbablyContainsSilence(buffer, frames)) {
    // Note: Prevent indefinite incrementing of |frames_silent_so_far_|, to
    // avoid eventual integer overflow.
    if (frames_silent_so_far_ < frames_before_observing_silence_) {
      frames_silent_so_far_ += frames;
      if (frames_silent_so_far_ >= frames_before_observing_silence_) {
        DCHECK(AtomicRefCountIsZero(&observing_silence_));
        AtomicRefCountInc(&observing_silence_);
      }
    }
  } else {
    if (frames_silent_so_far_ >= frames_before_observing_silence_) {
      DCHECK(AtomicRefCountIsOne(&observing_silence_));
      AtomicRefCountDec(&observing_silence_);
    }
    frames_silent_so_far_ = 0;
  }
}

void AudioSilenceDetector::MaybeInvokeAudibleCallback() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const bool is_now_audible = AtomicRefCountIsZero(&observing_silence_);
  if (was_audible_ && !is_now_audible)
    notify_is_audible_.Run(was_audible_ = false);
  else if (!was_audible_ && is_now_audible)
    notify_is_audible_.Run(was_audible_ = true);
}

bool AudioSilenceDetector::ProbablyContainsSilence(const AudioBus* buffer,
                                                   int num_frames) {
  if (!buffer)
    return true;
  DCHECK_LE(num_frames, buffer->frames());
  if (buffer->frames() <= 0)
    return true;

  // Scan the data in each channel.  If any one channel contains sound whose
  // range of values exceeds |silence_threshold_|, return false immediately.
  for (int i = 0; i < buffer->channels(); ++i) {
    // Examine the 1st, 2nd (+1), 4th (+2), 7th (+3), 11th (+4), etc. samples,
    // checking whether |silence_threshold_| has been breached each time.  For
    // typical AudioBus sizes, this algorithm will in the worst case examine
    // fewer than 10% of the samples.
    //
    // Note that there *is* a heavy bias in sampling at the beginning of the
    // channels, but that doesn't matter.  The buffer sizes are simply too
    // small.  For example, it is commonplace to use 128-sample buffers, which
    // represents ~3 ms of audio at 44.1 kHz; and this means that frequencies
    // below ~350 Hz will span more than one buffer to make a full cycle.  In
    // all, the algorithm here is meant to be dirt-simple math that isn't
    // susceptible to periodic bias within a single buffer.
    const float* p = buffer->channel(i);
    const float* const end_of_samples = p + num_frames;
    int skip = 1;
    float min_value = *p;
    float max_value = *p;
    for (p += skip; p < end_of_samples; ++skip, p += skip) {
      DCHECK(base::IsFinite(*p));
      if (*p < min_value)
        min_value = *p;
      else if (max_value < *p)
        max_value = *p;
      if ((max_value - min_value) > silence_threshold_)
        return false;
    }
  }

  return true;
}

}  // namespace media
