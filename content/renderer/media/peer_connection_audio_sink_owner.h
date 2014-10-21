// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_AUDIO_SINK_OWNER_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_AUDIO_SINK_OWNER_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "content/renderer/media/media_stream_audio_track_sink.h"

namespace content {

class PeerConnectionAudioSink;

// Reference counted holder of PeerConnectionAudioSink.
class PeerConnectionAudioSinkOwner : public MediaStreamAudioTrackSink {
 public:
  explicit PeerConnectionAudioSinkOwner(PeerConnectionAudioSink* sink);

  // MediaStreamAudioTrackSink implementation.
  int OnData(const int16* audio_data,
             int sample_rate,
             int number_of_channels,
             int number_of_frames,
             const std::vector<int>& channels,
             int audio_delay_milliseconds,
             int current_volume,
             bool need_audio_processing,
             bool key_pressed) override;
  void OnSetFormat(const media::AudioParameters& params) override;
  void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) override;
  void Reset() override;
  bool IsEqual(const MediaStreamAudioSink* other) const override;
  bool IsEqual(const PeerConnectionAudioSink* other) const override;

 protected:
  ~PeerConnectionAudioSinkOwner() override {}

 private:
  mutable base::Lock lock_;

  // Raw pointer to the delegate, the client need to call Reset() to set the
  // pointer to NULL before the delegate goes away.
  PeerConnectionAudioSink* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionAudioSinkOwner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_AUDIO_SINK_OWNER_H_
