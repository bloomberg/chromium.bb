// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_CLOCK_H_
#define MEDIA_FILTERS_AUDIO_CLOCK_H_

#include <deque>

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Models a queue of buffered audio in a playback pipeline for use with
// estimating the amount of delay in wall clock time. Takes changes in playback
// rate into account to handle scenarios where multiple rates may be present in
// a playback pipeline with large delay.
class MEDIA_EXPORT AudioClock {
 public:
  AudioClock(base::TimeDelta start_timestamp, int sample_rate);
  ~AudioClock();

  // |frames_written| amount of audio data scaled to |playback_rate| written.
  // |frames_requested| amount of audio data requested by hardware.
  // |delay_frames| is the current amount of hardware delay.
  void WroteAudio(int frames_written,
                  int frames_requested,
                  int delay_frames,
                  float playback_rate);

  // Calculates the current media timestamp taking silence and changes in
  // playback rate into account.
  base::TimeDelta current_media_timestamp() const {
    return current_media_timestamp_;
  }

  // Clients can provide |time_since_writing| to simulate the passage of time
  // since last writing audio to get a more accurate current media timestamp.
  base::TimeDelta CurrentMediaTimestampSinceWriting(
      base::TimeDelta time_since_writing) const;

  // Returns the amount of contiguous media time buffered at the head of the
  // audio hardware buffer. Silence introduced into the audio hardware buffer is
  // treated as a break in media time.
  base::TimeDelta contiguous_audio_data_buffered() const {
    return contiguous_audio_data_buffered_;
  }

  // Same as above, but also treats changes in playback rate as a break in media
  // time.
  base::TimeDelta contiguous_audio_data_buffered_at_same_rate() const {
    return contiguous_audio_data_buffered_at_same_rate_;
  }

  // Returns true if there is any audio data buffered by the audio hardware,
  // even if there is silence mixed in.
  bool audio_data_buffered() const { return audio_data_buffered_; }

 private:
  // Even with a ridiculously high sample rate of 256kHz, using 64 bits will
  // permit tracking up to 416999965 days worth of time (that's 1141 millenia).
  //
  // 32 bits on the other hand would top out at measly 2 hours and 20 minutes.
  struct AudioData {
    AudioData(int64_t frames, float playback_rate);

    int64_t frames;
    float playback_rate;
  };

  // Helpers for operating on |buffered_|.
  void PushBufferedAudioData(int64_t frames, float playback_rate);
  void PopBufferedAudioData(int64_t frames);
  base::TimeDelta ComputeBufferedMediaTime(int64_t frames) const;

  const base::TimeDelta start_timestamp_;
  const int sample_rate_;
  const double microseconds_per_frame_;

  std::deque<AudioData> buffered_;
  int64_t total_buffered_frames_;

  base::TimeDelta current_media_timestamp_;

  // Cached results of last call to WroteAudio().
  bool audio_data_buffered_;
  base::TimeDelta contiguous_audio_data_buffered_;
  base::TimeDelta contiguous_audio_data_buffered_at_same_rate_;

  DISALLOW_COPY_AND_ASSIGN(AudioClock);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_CLOCK_H_
