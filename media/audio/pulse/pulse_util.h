// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_PULSE_UTIL_H_
#define MEDIA_AUDIO_PULSE_PULSE_UTIL_H_

#include <pulse/pulseaudio.h>

#include "base/basictypes.h"
#include "media/audio/audio_device_name.h"
#include "media/base/channel_layout.h"

namespace media {

namespace pulse {

// A helper class that acquires pa_threaded_mainloop_lock() while in scope.
class AutoPulseLock {
 public:
  explicit AutoPulseLock(pa_threaded_mainloop* pa_mainloop)
      : pa_mainloop_(pa_mainloop) {
    pa_threaded_mainloop_lock(pa_mainloop_);
  }

  ~AutoPulseLock() {
    pa_threaded_mainloop_unlock(pa_mainloop_);
  }

 private:
  pa_threaded_mainloop* pa_mainloop_;
  DISALLOW_COPY_AND_ASSIGN(AutoPulseLock);
};

// Triggers pa_threaded_mainloop_signal() to avoid deadlocks.
void StreamSuccessCallback(pa_stream* s, int error, void* mainloop);
void ContextStateCallback(pa_context* context, void* mainloop);

pa_sample_format_t BitsToPASampleFormat(int bits_per_sample);

pa_channel_map ChannelLayoutToPAChannelMap(ChannelLayout channel_layout);

void WaitForOperationCompletion(pa_threaded_mainloop* pa_mainloop,
                                pa_operation* operation);

int GetHardwareLatencyInBytes(pa_stream* stream,
                              int sample_rate,
                              int bytes_per_frame);

}  // namespace pulse

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_UTIL_H_
