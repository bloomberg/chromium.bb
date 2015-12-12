// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_

#include "base/memory/ref_counted.h"
#include "content/renderer/media/media_stream_audio_track.h"

namespace content {

// MediaStreamRemoteAudioTrack is a WebRTC specific implementation of an
// audio track received from a PeerConnection.
// TODO(tommi): Chrome shouldn't have to care about remote vs local so
// we should have a single track implementation that delegates to the
// sources that do different things depending on the type of source.
class MediaStreamRemoteAudioTrack : public MediaStreamAudioTrack {
 public:
  explicit MediaStreamRemoteAudioTrack(
      const scoped_refptr<webrtc::AudioTrackInterface>& track);
  ~MediaStreamRemoteAudioTrack() override;

  void SetEnabled(bool enabled) override;
  void Stop() override;

  void AddSink(MediaStreamAudioSink* sink) override;
  void RemoveSink(MediaStreamAudioSink* sink) override;
  media::AudioParameters GetOutputFormat() const override;

  webrtc::AudioTrackInterface* GetAudioAdapter() override;

 private:
  class AudioSink;
  scoped_ptr<AudioSink> sink_;
  const scoped_refptr<webrtc::AudioTrackInterface> track_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_
