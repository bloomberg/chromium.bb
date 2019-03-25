// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_UTILS_H_
#define CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_UTILS_H_

#include <memory>

#include "content/common/content_export.h"
#include "media/capture/video_capture_types.h"

namespace blink {
class WebMediaStream;
}

namespace media {
class VideoCapturerSource;
}

namespace content {
// This method creates a WebMediaStreamSource + MediaStreamSource pair with the
// provided video capturer source. A new WebMediaStreamTrack +
// MediaStreamTrack pair is created, connected to the source and is plugged into
// the WebMediaStream (|web_media_stream|).
// |is_remote| should be true if the source of the data is not a local device.
// |is_readonly| should be true if the format of the data cannot be changed by
//     MediaTrackConstraints.
CONTENT_EXPORT bool AddVideoTrackToMediaStream(
    std::unique_ptr<media::VideoCapturerSource> video_source,
    bool is_remote,
    blink::WebMediaStream* web_media_stream);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_UTILS_H_
