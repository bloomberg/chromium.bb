// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SOURCE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

class CONTENT_EXPORT MediaStreamAudioSource
    : NON_EXPORTED_BASE(public MediaStreamSource) {
 public:
  MediaStreamAudioSource(int render_frame_id,
                         const StreamDeviceInfo& device_info,
                         const SourceStoppedCallback& stop_callback,
                         PeerConnectionDependencyFactory* factory);
  MediaStreamAudioSource();
  ~MediaStreamAudioSource() override;

  void AddTrack(const blink::WebMediaStreamTrack& track,
                const blink::WebMediaConstraints& constraints,
                const ConstraintsCallback& callback);

  void SetLocalAudioSource(webrtc::AudioSourceInterface* source) {
    local_audio_source_ = source;
  }

  void SetAudioCapturer(const scoped_refptr<WebRtcAudioCapturer>& capturer) {
    DCHECK(!audio_capturer_.get());
    audio_capturer_ = capturer;
  }

  const scoped_refptr<WebRtcAudioCapturer>& GetAudioCapturer() {
    return audio_capturer_;
  }

  webrtc::AudioSourceInterface* local_audio_source() {
    return local_audio_source_.get();
  }

 protected:
  void DoStopSource() override;

 private:
  const int render_frame_id_;
  PeerConnectionDependencyFactory* const factory_;

  // This member holds an instance of webrtc::LocalAudioSource. This is used
  // as a container for audio options.
  scoped_refptr<webrtc::AudioSourceInterface> local_audio_source_;

  scoped_refptr<WebRtcAudioCapturer> audio_capturer_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SOURCE_H_
