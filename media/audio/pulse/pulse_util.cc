// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/pulse_util.h"

#include "base/logging.h"
#include "base/time.h"
#include "media/audio/audio_manager_base.h"

namespace media {

namespace pulse {

namespace {

pa_channel_position ChromiumToPAChannelPosition(Channels channel) {
  switch (channel) {
    // PulseAudio does not differentiate between left/right and
    // stereo-left/stereo-right, both translate to front-left/front-right.
    case LEFT:
      return PA_CHANNEL_POSITION_FRONT_LEFT;
    case RIGHT:
      return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case CENTER:
      return PA_CHANNEL_POSITION_FRONT_CENTER;
    case LFE:
      return PA_CHANNEL_POSITION_LFE;
    case BACK_LEFT:
      return PA_CHANNEL_POSITION_REAR_LEFT;
    case BACK_RIGHT:
      return PA_CHANNEL_POSITION_REAR_RIGHT;
    case LEFT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case RIGHT_OF_CENTER:
      return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case BACK_CENTER:
      return PA_CHANNEL_POSITION_REAR_CENTER;
    case SIDE_LEFT:
      return PA_CHANNEL_POSITION_SIDE_LEFT;
    case SIDE_RIGHT:
      return PA_CHANNEL_POSITION_SIDE_RIGHT;
    case CHANNELS_MAX:
      return PA_CHANNEL_POSITION_INVALID;
    default:
      NOTREACHED() << "Invalid channel: " << channel;
      return PA_CHANNEL_POSITION_INVALID;
  }
}

}  // namespace

// static, pa_stream_success_cb_t
void StreamSuccessCallback(pa_stream* s, int error, void* mainloop) {
  pa_threaded_mainloop* pa_mainloop =
      static_cast<pa_threaded_mainloop*>(mainloop);
  pa_threaded_mainloop_signal(pa_mainloop, 0);
}

// |pa_context| and |pa_stream| state changed cb.
void ContextStateCallback(pa_context* context, void* mainloop) {
  pa_threaded_mainloop* pa_mainloop =
      static_cast<pa_threaded_mainloop*>(mainloop);
  pa_threaded_mainloop_signal(pa_mainloop, 0);
}

pa_sample_format_t BitsToPASampleFormat(int bits_per_sample) {
  switch (bits_per_sample) {
    case 8:
      return PA_SAMPLE_U8;
    case 16:
      return PA_SAMPLE_S16LE;
    case 24:
      return PA_SAMPLE_S24LE;
    case 32:
      return PA_SAMPLE_S32LE;
    default:
      NOTREACHED() << "Invalid bits per sample: " << bits_per_sample;
      return PA_SAMPLE_INVALID;
  }
}

pa_channel_map ChannelLayoutToPAChannelMap(ChannelLayout channel_layout) {
  pa_channel_map channel_map;
  pa_channel_map_init(&channel_map);

  channel_map.channels = ChannelLayoutToChannelCount(channel_layout);
  for (Channels ch = LEFT; ch < CHANNELS_MAX;
       ch = static_cast<Channels>(ch + 1)) {
    int channel_index = ChannelOrder(channel_layout, ch);
    if (channel_index < 0)
      continue;

    channel_map.map[channel_index] = ChromiumToPAChannelPosition(ch);
  }

  return channel_map;
}

void WaitForOperationCompletion(pa_threaded_mainloop* pa_mainloop,
                                pa_operation* operation) {
  if (!operation) {
    DLOG(WARNING) << "Operation is NULL";
    return;
  }

  while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
    pa_threaded_mainloop_wait(pa_mainloop);

  pa_operation_unref(operation);
}

int GetHardwareLatencyInBytes(pa_stream* stream,
                              int sample_rate,
                              int bytes_per_frame) {
  DCHECK(stream);
  int negative = 0;
  pa_usec_t latency_micros = 0;
  if (pa_stream_get_latency(stream, &latency_micros, &negative) != 0)
    return 0;

  if (negative)
    return 0;

  return latency_micros * sample_rate * bytes_per_frame /
      base::Time::kMicrosecondsPerSecond;
}

}  // namespace pulse

}  // namespace media
