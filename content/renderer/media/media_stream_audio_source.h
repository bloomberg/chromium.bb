// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SOURCE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

class MediaStreamAudioTrack;

// TODO(miu): In a soon-upcoming set of refactoring changes, this class will
// become a base class for managing tracks (part of what WebRtcAudioCapturer
// does today).  Then, the rest of WebRtcAudioCapturer will be rolled into a
// subclass.   http://crbug.com/577874
class CONTENT_EXPORT MediaStreamAudioSource
    : NON_EXPORTED_BASE(public MediaStreamSource) {
 public:
  MediaStreamAudioSource(int render_frame_id,
                         const StreamDeviceInfo& device_info,
                         const SourceStoppedCallback& stop_callback,
                         PeerConnectionDependencyFactory* factory);
  MediaStreamAudioSource();
  ~MediaStreamAudioSource() override;

  // Returns the MediaStreamAudioSource instance owned by the given blink
  // |source| or null.
  static MediaStreamAudioSource* From(const blink::WebMediaStreamSource& track);

  void AddTrack(const blink::WebMediaStreamTrack& track,
                const blink::WebMediaConstraints& constraints,
                const ConstraintsCallback& callback);

  base::WeakPtr<MediaStreamAudioSource> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Removes |track| from the list of instances that get a copy of the source
  // audio data.
  void StopAudioDeliveryTo(MediaStreamAudioTrack* track);

  WebRtcAudioCapturer* audio_capturer() const { return audio_capturer_.get(); }

  void SetAudioCapturer(scoped_ptr<WebRtcAudioCapturer> capturer) {
    DCHECK(!audio_capturer_.get());
    audio_capturer_ = std::move(capturer);
  }

  webrtc::AudioSourceInterface* local_audio_source() {
    return local_audio_source_.get();
  }

  void SetLocalAudioSource(scoped_refptr<webrtc::AudioSourceInterface> source) {
    local_audio_source_ = std::move(source);
  }

  WebAudioCapturerSource* webaudio_capturer() const {
    return webaudio_capturer_.get();
  }

  void SetWebAudioCapturer(scoped_ptr<WebAudioCapturerSource> capturer) {
    DCHECK(!webaudio_capturer_.get());
    webaudio_capturer_ = std::move(capturer);
  }

 protected:
  void DoStopSource() override;

 private:
  const int render_frame_id_;
  PeerConnectionDependencyFactory* const factory_;

  // MediaStreamAudioSource is the owner of either a WebRtcAudioCapturer or a
  // WebAudioCapturerSource.
  //
  // TODO(miu): In a series of soon-upcoming changes, WebRtcAudioCapturer and
  // WebAudioCapturerSource will become subclasses of MediaStreamAudioSource
  // instead.
  scoped_ptr<WebRtcAudioCapturer> audio_capturer_;
  scoped_ptr<WebAudioCapturerSource> webaudio_capturer_;

  // This member holds an instance of webrtc::LocalAudioSource. This is used
  // as a container for audio options.
  scoped_refptr<webrtc::AudioSourceInterface> local_audio_source_;

  // Provides weak pointers so that MediaStreamAudioTracks won't call
  // StopAudioDeliveryTo() if this instance dies first.
  base::WeakPtrFactory<MediaStreamAudioSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_SOURCE_H_
