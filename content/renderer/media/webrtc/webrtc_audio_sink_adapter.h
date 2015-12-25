// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_SINK_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_SINK_ADAPTER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "media/audio/audio_parameters.h"

namespace webrtc {
class AudioTrackSinkInterface;
}  // namespace webrtc

namespace content {

// Adapter to the webrtc::AudioTrackSinkInterface of the audio track.
// This class is used in between the MediaStreamAudioSink and
// webrtc::AudioTrackSinkInterface. It gets data callback via the
// MediaStreamAudioSink::OnData() interface and pass the data to
// webrtc::AudioTrackSinkInterface.
class WebRtcAudioSinkAdapter : public MediaStreamAudioSink {
 public:
  explicit WebRtcAudioSinkAdapter(
      webrtc::AudioTrackSinkInterface* sink);
  ~WebRtcAudioSinkAdapter() override;

  bool IsEqual(const webrtc::AudioTrackSinkInterface* other) const;

 private:
  // MediaStreamAudioSink implementation.
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override;
  void OnSetFormat(const media::AudioParameters& params) override;

  webrtc::AudioTrackSinkInterface* const sink_;

  media::AudioParameters params_;
  scoped_ptr<int16_t[]> interleaved_data_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioSinkAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_SINK_ADAPTER_H_
