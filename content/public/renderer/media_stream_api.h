// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_API_H_
#define CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_API_H_

#include "content/common/content_export.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/video_capturer_source.h"

namespace blink {
class WebMediaStreamSource;
}

namespace Media {
class AudioParameters;
}

namespace content {

// These two methods will initialize a WebMediaStreamSource object to take
// data from the provided audio or video capturer source.
// |is_remote| should be true if the source of the data is not a local device.
// |is_readonly| should be true if the format of the data cannot be changed by
//     MediaTrackConstraints.
CONTENT_EXPORT bool AddVideoTrackToMediaStream(
    scoped_ptr<media::VideoCapturerSource> source,
    bool is_remote,
    bool is_readonly,
    const std::string& media_stream_url);
CONTENT_EXPORT bool AddAudioTrackToMediaStream(
    scoped_refptr<media::AudioCapturerSource> source,
    const media::AudioParameters& params,
    bool is_remote,
    bool is_readonly,
    const std::string& media_stream_url);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_API_H_
