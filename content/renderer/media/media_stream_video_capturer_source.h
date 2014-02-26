// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_video_source.h"

namespace content {

// MediaStreamVideoCapturerSource is an implementation of
// MediaStreamVideoSource used for local video capture such as USB cameras
// and tab capture.
// TODO(perkj): Currently, this use RTCVideoCapturer and a libjingle
// implementation of webrt::MediaStreamSourceInterface. Implement this class
// without using cricket::VideoCapturer and webrtc::VideoSourceInterface as
// part of project Piranha Plant.
class CONTENT_EXPORT MediaStreamVideoCapturerSource
    : public MediaStreamVideoSource {
 public:
  MediaStreamVideoCapturerSource(
      const StreamDeviceInfo& device_info,
      const SourceStoppedCallback& stop_callback,
      MediaStreamDependencyFactory* factory);
  virtual ~MediaStreamVideoCapturerSource();

 protected:
  virtual void InitAdapter(
      const blink::WebMediaConstraints& constraints) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
