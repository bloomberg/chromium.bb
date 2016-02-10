// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/renderer/media/webrtc/webrtc_audio_sink_adapter.h"
#include "media/base/audio_bus.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

WebRtcAudioSinkAdapter::WebRtcAudioSinkAdapter(
    webrtc::AudioTrackSinkInterface* sink)
    : sink_(sink) {
  DCHECK(sink);
}

WebRtcAudioSinkAdapter::~WebRtcAudioSinkAdapter() {
}

bool WebRtcAudioSinkAdapter::IsEqual(
    const webrtc::AudioTrackSinkInterface* other) const {
  return (other == sink_);
}

void WebRtcAudioSinkAdapter::OnData(const media::AudioBus& audio_bus,
                                    base::TimeTicks estimated_capture_time) {
  DCHECK_EQ(audio_bus.frames(), params_.frames_per_buffer());
  DCHECK_EQ(audio_bus.channels(), params_.channels());
  // TODO(henrika): Remove this conversion once the interface in libjingle
  // supports float vectors.
  audio_bus.ToInterleaved(audio_bus.frames(),
                          sizeof(interleaved_data_[0]),
                          interleaved_data_.get());
  sink_->OnData(interleaved_data_.get(),
                16,
                params_.sample_rate(),
                audio_bus.channels(),
                audio_bus.frames());
}

void WebRtcAudioSinkAdapter::OnSetFormat(
    const media::AudioParameters& params) {
  DCHECK(params.IsValid());
  params_ = params;
  const int num_pcm16_data_elements =
      params_.frames_per_buffer() * params_.channels();
  interleaved_data_.reset(new int16_t[num_pcm16_data_elements]);
}

}  // namespace content
