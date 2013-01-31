// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/base/audio_capturer_source.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"

namespace content {

class CONTENT_EXPORT MediaStreamSourceExtraData
    : NON_EXPORTED_BASE(public WebKit::WebMediaStreamSource::ExtraData) {
 public:

  explicit MediaStreamSourceExtraData(
      const StreamDeviceInfo& device_info);
  explicit MediaStreamSourceExtraData(
      media::AudioCapturerSource* source);
  virtual ~MediaStreamSourceExtraData();

  // Return device information about the camera or microphone.
  const StreamDeviceInfo& device_info() const {
    return device_info_;
  }

  void SetVideoSource(webrtc::VideoSourceInterface* source) {
    video_source_ = source;
  }

  webrtc::VideoSourceInterface* video_source() { return video_source_; }
  media::AudioCapturerSource* audio_source() { return audio_source_; }

 private:
  StreamDeviceInfo device_info_;
  scoped_refptr<webrtc::VideoSourceInterface> video_source_;
  scoped_refptr<media::AudioCapturerSource> audio_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamSourceExtraData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_SOURCE_EXTRA_DATA_H_
