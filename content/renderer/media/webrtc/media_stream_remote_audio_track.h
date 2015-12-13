// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

namespace content {

class MediaStreamRemoteAudioSource;

// MediaStreamRemoteAudioTrack is a WebRTC specific implementation of an
// audio track received from a PeerConnection.
// TODO(tommi): Chrome shouldn't have to care about remote vs local so
// we should have a single track implementation that delegates to the
// sources that do different things depending on the type of source.
class MediaStreamRemoteAudioTrack : public MediaStreamAudioTrack {
 public:
  explicit MediaStreamRemoteAudioTrack(
      const blink::WebMediaStreamSource& source, bool enabled);
  ~MediaStreamRemoteAudioTrack() override;

  void SetEnabled(bool enabled) override;
  void Stop() override;

  void AddSink(MediaStreamAudioSink* sink) override;
  void RemoveSink(MediaStreamAudioSink* sink) override;
  media::AudioParameters GetOutputFormat() const override;

  webrtc::AudioTrackInterface* GetAudioAdapter() override;

 private:
  MediaStreamRemoteAudioSource* source() const;

  blink::WebMediaStreamSource source_;
  bool enabled_;
};

// Inheriting from ExtraData directly since MediaStreamAudioSource has
// too much unrelated bloat.
// TODO(tommi): MediaStreamAudioSource needs refactoring.
class MediaStreamRemoteAudioSource
    : public blink::WebMediaStreamSource::ExtraData {
 public:
  explicit MediaStreamRemoteAudioSource(
      const scoped_refptr<webrtc::AudioTrackInterface>& track);
  ~MediaStreamRemoteAudioSource() override;

  // Controls whether or not the source is included in the main, mixed, audio
  // output from WebRTC as rendered by WebRtcAudioRenderer (media players).
  void SetEnabledForMixing(bool enabled);

  // Adds an audio sink for a track belonging to this source.
  // |enabled| is the enabled state of the track and can be updated via
  // a call to SetSinksEnabled.
  void AddSink(MediaStreamAudioSink* sink, MediaStreamAudioTrack* track,
               bool enabled);

  // Removes an audio sink for a track belonging to this source.
  void RemoveSink(MediaStreamAudioSink* sink, MediaStreamAudioTrack* track);

  // Turns audio callbacks on/off for all sinks belonging to a track.
  void SetSinksEnabled(MediaStreamAudioTrack* track, bool enabled);

  // Removes all sinks belonging to a track.
  void RemoveAll(MediaStreamAudioTrack* track);

  webrtc::AudioTrackInterface* GetAudioAdapter();

 private:
  class AudioSink;
  scoped_ptr<AudioSink> sink_;
  const scoped_refptr<webrtc::AudioTrackInterface> track_;
  base::ThreadChecker thread_checker_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_AUDIO_TRACK_H_
