// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_POWER_MONITOR_H_
#define MEDIA_AUDIO_AUDIO_POWER_MONITOR_H_

#include <limits>

#include "base/callback.h"
#include "media/base/media_export.h"

// An audio signal power monitor.  It is periodically provided an AudioBus by
// the native audio thread, and the audio samples in each channel are analyzed
// to determine the average power of the signal over a time period.  Here
// "average power" is a running average calculated by using a first-order
// low-pass filter over the square of the samples scanned.  Whenever reporting
// the power level, this running average is converted to dBFS (decibels relative
// to full-scale) units.
//
// Note that extreme care has been taken to make the AudioPowerMonitor::Scan()
// method safe to be called on the native audio thread.  The code acquires no
// locks, nor engages in any operation that could result in an
// undetermined/unbounded amount of run-time.

namespace base {
class MessageLoop;
class TimeDelta;
}

namespace media {

class AudioBus;

class MEDIA_EXPORT AudioPowerMonitor {
 public:
  // Reports power level in terms of dBFS (see zero_power() and max_power()
  // below).  |clipped| is true if any *one* sample exceeded maximum amplitude
  // since the last invocation.
  typedef base::Callback<void(float power_dbfs, bool clipped)>
      PowerMeasurementCallback;

  // |sample_rate| is the audio signal sample rate (Hz).  |time_constant|
  // characterizes how samples are averaged over time to determine the power
  // level; and is the amount of time it takes a zero power level to increase to
  // ~63.2% of maximum given a step input signal.  |measurement_period| is the
  // time length of signal to analyze before invoking the callback to report the
  // current power level.  |message_loop| is where the |callback| task will be
  // posted.
  AudioPowerMonitor(int sample_rate,
                    const base::TimeDelta& time_constant,
                    const base::TimeDelta& measurement_period,
                    base::MessageLoop* message_loop,
                    const PowerMeasurementCallback& callback);

  ~AudioPowerMonitor();

  // Scan more |frames| of audio data from |buffer|.  It is safe to call this
  // from a real-time priority thread.
  void Scan(const AudioBus& buffer, int frames);

  // dBFS value corresponding to zero power in the audio signal.
  static float zero_power() { return -std::numeric_limits<float>::infinity(); }

  // dBFS value corresponding to maximum power in the audio signal.
  static float max_power() { return 0.0f; }

 private:
  // The weight applied when averaging-in each sample.  Computed from the
  // |sample_rate| and |time_constant|.
  const float sample_weight_;

  // Number of audio frames to be scanned before reporting the current power
  // level via callback, as computed from |sample_rate| and
  // |measurement_period|.
  const int num_frames_per_callback_;

  // MessageLoop and callback used to notify of the current power level.
  base::MessageLoop* const message_loop_;
  const PowerMeasurementCallback power_level_callback_;

  // Accumulated results over one or more calls to Scan().
  float average_power_;
  bool clipped_since_last_notification_;
  int frames_since_last_notification_;

  // Keep track of last reported results to forgo making redundant callbacks.
  float last_reported_power_;
  bool last_reported_clipped_;

  DISALLOW_COPY_AND_ASSIGN(AudioPowerMonitor);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_POWER_MONITOR_H_
