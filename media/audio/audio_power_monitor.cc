// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_power_monitor.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/float_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"

namespace media {

AudioPowerMonitor::AudioPowerMonitor(
    int sample_rate,
    const base::TimeDelta& time_constant,
    const base::TimeDelta& measurement_period,
    base::MessageLoop* message_loop,
    const PowerMeasurementCallback& callback)
    : sample_weight_(
          1.0f - expf(-1.0f / (sample_rate * time_constant.InSecondsF()))),
      num_frames_per_callback_(sample_rate * measurement_period.InSecondsF()),
      message_loop_(message_loop),
      power_level_callback_(callback),
      average_power_(0.0f),
      clipped_since_last_notification_(false),
      frames_since_last_notification_(0),
      last_reported_power_(-1.0f),
      last_reported_clipped_(false) {
  DCHECK(message_loop_);
  DCHECK(!power_level_callback_.is_null());
}

AudioPowerMonitor::~AudioPowerMonitor() {
}

void AudioPowerMonitor::Scan(const AudioBus& buffer, int num_frames) {
  DCHECK_LE(num_frames, buffer.frames());
  const int num_channels = buffer.channels();
  if (num_frames <= 0 || num_channels <= 0)
    return;

  // Calculate a new average power by applying a first-order low-pass filter
  // over the audio samples in |buffer|.
  //
  // TODO(miu): Implement optimized SSE/NEON to more efficiently compute the
  // results (in media/base/vector_math) in soon-upcoming change.
  bool clipped = false;
  float sum_power = 0.0f;
  for (int i = 0; i < num_channels; ++i) {
    float average_power_this_channel = average_power_;
    const float* p = buffer.channel(i);
    const float* const end_of_samples = p + num_frames;
    for (; p < end_of_samples; ++p) {
      const float sample = *p;
      const float sample_squared = sample * sample;
      clipped |= (sample_squared > 1.0f);
      average_power_this_channel +=
          (sample_squared - average_power_this_channel) * sample_weight_;
    }
    // If data in audio buffer is garbage, ignore its effect on the result.
    if (base::IsNaN(average_power_this_channel))
      average_power_this_channel = average_power_;
    sum_power += average_power_this_channel;
  }

  // Update accumulated results.
  average_power_ = std::max(0.0f, std::min(1.0f, sum_power / num_channels));
  clipped_since_last_notification_ |= clipped;
  frames_since_last_notification_ += num_frames;

  // Once enough frames have been scanned, report the accumulated results.
  if (frames_since_last_notification_ >= num_frames_per_callback_) {
    // Note: Forgo making redundant callbacks when results remain unchanged.
    // Part of this is to pin-down the power to zero if it is insignificantly
    // small.
    const float kInsignificantPower = 1.0e-10f;  // -100 dBFS
    const float power =
        (average_power_ < kInsignificantPower) ? 0.0f : average_power_;
    if (power != last_reported_power_ ||
        clipped_since_last_notification_ != last_reported_clipped_) {
      const float power_dbfs =
          power > 0.0f ? 10.0f * log10f(power) : zero_power();
      // Try to post a task to run the callback with the dBFS result.  The
      // posting of the task is guaranteed to be non-blocking, and therefore
      // could fail.  However, in the common case, failures should be rare (and
      // then the task-post will likely succeed the next time it's attempted).
      if (!message_loop_->TryPostTask(
              FROM_HERE,
              base::Bind(power_level_callback_,
                         power_dbfs, clipped_since_last_notification_))) {
        DVLOG(2) << "TryPostTask() did not succeed.";
        return;
      }
      last_reported_power_ = power;
      last_reported_clipped_ = clipped_since_last_notification_;
    }
    clipped_since_last_notification_ = false;
    frames_since_last_notification_ = 0;
  }
}

}  // namespace media
