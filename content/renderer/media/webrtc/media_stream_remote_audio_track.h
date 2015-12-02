// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_

#include "base/memory/ref_counted.h"
#include "content/renderer/media/media_stream_track.h"

namespace content {

// MediaStreamRemoteAudioTrack is a WebRTC specific implementation of an
// audio track received from a PeerConnection.
class MediaStreamRemoteAudioTrack : public MediaStreamTrack {
 public:
  explicit MediaStreamRemoteAudioTrack(
      const scoped_refptr<webrtc::AudioTrackInterface>& track);
  ~MediaStreamRemoteAudioTrack() override;

  void SetEnabled(bool enabled) override;
  void Stop() override;

  webrtc::AudioTrackInterface* GetAudioAdapter() override;

 private:
  const scoped_refptr<webrtc::AudioTrackInterface> track_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_
