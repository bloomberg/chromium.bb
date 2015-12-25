// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SINK_OWNER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SINK_OWNER_H_

#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/renderer/media/media_stream_audio_track_sink.h"

namespace content {

class MediaStreamAudioSink;

// Reference counted holder of MediaStreamAudioSink sinks.
class MediaStreamAudioSinkOwner : public MediaStreamAudioTrackSink {
 public:
  explicit MediaStreamAudioSinkOwner(MediaStreamAudioSink* sink);

  // MediaStreamAudioTrackSink implementation.
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override;
  void OnSetFormat(const media::AudioParameters& params) override;
  void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) override;
  void Reset() override;
  bool IsEqual(const MediaStreamAudioSink* other) const override;

 protected:
  ~MediaStreamAudioSinkOwner() override {}

 private:
  mutable base::Lock lock_;

  // Raw pointer to the delegate, the client need to call Reset() to set the
  // pointer to NULL before the delegate goes away.
  MediaStreamAudioSink* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioSinkOwner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SINK_OWNER_H_
