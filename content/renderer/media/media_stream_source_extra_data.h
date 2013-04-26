// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_source_observer.h"
#include "media/base/audio_capturer_source.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"

namespace content {

class CONTENT_EXPORT MediaStreamSourceExtraData
    : NON_EXPORTED_BASE(public WebKit::WebMediaStreamSource::ExtraData) {
 public:
  MediaStreamSourceExtraData(
      const StreamDeviceInfo& device_info,
      const WebKit::WebMediaStreamSource& webkit_source);
  explicit MediaStreamSourceExtraData(
      media::AudioCapturerSource* source);
  virtual ~MediaStreamSourceExtraData();

  // Returns the WebMediaStreamSource object that owns this object.
  const WebKit::WebMediaStreamSource& webkit_source() const {
    return webkit_source_;
  }

  // Return device information about the camera or microphone.
  const StreamDeviceInfo& device_info() const {
    return device_info_;
  }

  void SetVideoSource(webrtc::VideoSourceInterface* source) {
    video_source_ = source;
    source_observer_.reset(new MediaStreamSourceObserver(source, this));
  }

  void SetLocalAudioSource(webrtc::AudioSourceInterface* source) {
    local_audio_source_ = source;
    // TODO(perkj): Implement a local source observer for audio.
    // See |source_observer_|.
  }

  webrtc::VideoSourceInterface* video_source() { return video_source_; }
  media::AudioCapturerSource* audio_source() { return audio_source_; }
  webrtc::AudioSourceInterface* local_audio_source() {
    return local_audio_source_;
  }

 private:
  StreamDeviceInfo device_info_;

  // TODO(tommyw): Remove |webkit_source_| after WebMediaStreamSource::Owner()
  // is implemented, which let us fetch the
  // WebMediaStreamSource without increasing the reference count.
  // |webkit_source_| will create a circular reference to WebMediaStreamSource.
  // WebMediaStreamSource -> MediaStreamSourceExtraData -> WebMediaStreamSource
  // Currently, we rely on manually releasing the MediaStreamSourceExtraData
  // from WebMediaStreamSource like what
  // MediaStreamImpl::~UserMediaRequestInfo() does.
  WebKit::WebMediaStreamSource webkit_source_;
  scoped_refptr<webrtc::VideoSourceInterface> video_source_;
  scoped_refptr<media::AudioCapturerSource> audio_source_;

  // This member holds an instance of webrtc::LocalAudioSource. This is used
  // as a container for audio options.
  // TODO(hclam): This should be merged with |audio_source_| such that it
  // carries audio options.
  scoped_refptr<webrtc::AudioSourceInterface> local_audio_source_;
  scoped_ptr<MediaStreamSourceObserver> source_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamSourceExtraData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_
