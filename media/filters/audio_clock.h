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
//
//
// USAGE
//
// Prior to starting audio playback, construct an AudioClock with an initial
// media timestamp and a sample rate matching the sample rate the audio device
// was opened at.
//
// Each time the audio rendering callback is executed, call WroteAudio() once
// (and only once!) containing information on what was written:
//   1) How many frames of audio data requested
//   2) How many frames of audio data provided
//   3) The playback rate of the audio data provided
//   4) The current amount of delay
//
// After a call to WroteAudio(), clients can inspect the resulting media
// timestamp. This can be used for UI purposes, synchronizing video, etc...
//
//
// DETAILS
//
// Silence (whether caused by the initial audio delay or failing to write the
// amount of requested frames due to underflow) is also modeled and will cause
// the media timestamp to stop increasing until all known silence has been
// played. AudioClock's model is initialized with silence during the first call
// to WroteAudio() using the delay value.
//
// Playback rates are tracked for translating frame durations into media
// durations. Since silence doesn't affect media timestamps, it also isn't
// affected by playback rates.
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

  // Returns the bounds of media data currently buffered by the audio hardware,
  // taking silence and changes in playback rate into account. Buffered audio
  // structure and timestamps are updated with every call to WroteAudio().
  //
  //  start_timestamp = 1000 ms                           sample_rate = 40 Hz
  // +-----------------------+-----------------------+-----------------------+
  // |   10 frames silence   |   20 frames @ 1.0x    |   20 frames @ 0.5x    |
  // |      = 250 ms (wall)  |      = 500 ms (wall)  |      = 500 ms (wall)  |
  // |      =   0 ms (media) |      = 500 ms (media) |      = 250 ms (media) |
  // +-----------------------+-----------------------+-----------------------+
  // ^                                                                       ^
  // front_timestamp() is equal to               back_timestamp() is equal to
  // |start_timestamp| since no                  amount of media frames tracked
  // media data has been played yet.             by AudioClock, which would be
  //                                             1000 + 500 + 250 = 1750 ms.
  base::TimeDelta front_timestamp() const { return front_timestamp_; }
  base::TimeDelta back_timestamp() const { return back_timestamp_; }

  // Clients can provide |time_since_writing| to simulate the passage of time
  // since last writing audio to get a more accurate current media timestamp.
  //
  // The value will be bounded between front_timestamp() and back_timestamp().
  base::TimeDelta TimestampSinceWriting(
      base::TimeDelta time_since_writing) const;

  // Returns the amount of wall time until |timestamp| will be played by the
  // audio hardware.
  //
  // |timestamp| must be within front_timestamp() and back_timestamp().
  base::TimeDelta TimeUntilPlayback(base::TimeDelta timestamp) const;

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

  base::TimeDelta front_timestamp_;
  base::TimeDelta back_timestamp_;

  // Cached results of last call to WroteAudio().
  base::TimeDelta contiguous_audio_data_buffered_;
  base::TimeDelta contiguous_audio_data_buffered_at_same_rate_;

  DISALLOW_COPY_AND_ASSIGN(AudioClock);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_CLOCK_H_
