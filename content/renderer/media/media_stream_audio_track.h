// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_

#include "base/macros.h"
#include "content/renderer/media/media_stream_track.h"

namespace webrtc {
class AudioTrackInterface;
}  // namespace webrtc

namespace content {

class MediaStreamAudioSink;

class CONTENT_EXPORT MediaStreamAudioTrack : public MediaStreamTrack {
 public:
  explicit MediaStreamAudioTrack(bool is_local_track);
  ~MediaStreamAudioTrack() override;

  static MediaStreamAudioTrack* GetTrack(
      const blink::WebMediaStreamTrack& track);

  // Add a sink to the track. This function will trigger a OnSetFormat()
  // call on the |sink|.
  // Called on the main render thread.
  virtual void AddSink(MediaStreamAudioSink* sink) = 0;

  // Remove a sink from the track.
  // Called on the main render thread.
  virtual void RemoveSink(MediaStreamAudioSink* sink) = 0;

  // TODO(tommi, xians): Remove this method.
  virtual webrtc::AudioTrackInterface* GetAudioAdapter();

  // Returns the output format of the capture source. May return an invalid
  // AudioParameters if the format is not yet available.
  // Called on the main render thread.
  // TODO(tommi): This method appears to only be used by Pepper and in fact
  // does not appear to be necessary there.  We should remove it since it adds
  // to the complexity of all types of audio tracks+source implementations.
  virtual media::AudioParameters GetOutputFormat() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_
