// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_UTILS_H_
#define CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_UTILS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "media/base/channel_layout.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"

namespace blink {
class WebMediaStream;
class WebMediaStreamSink;
class WebMediaStreamTrack;
}

namespace media {
class AudioCapturerSource;
class VideoCapturerSource;
}

namespace content {
// These methods create a WebMediaStreamSource + MediaStreamSource pair with the
// provided audio or video capturer source. A new WebMediaStreamTrack +
// MediaStreamTrack pair is created, connected to the source and is plugged into
// the WebMediaStream (|web_media_stream|).
// |is_remote| should be true if the source of the data is not a local device.
// |is_readonly| should be true if the format of the data cannot be changed by
//     MediaTrackConstraints.
CONTENT_EXPORT bool AddVideoTrackToMediaStream(
    std::unique_ptr<media::VideoCapturerSource> video_source,
    bool is_remote,
    blink::WebMediaStream* web_media_stream);

// |sample_rate|, |channel_layout|, and |frames_per_buffer| specify the audio
// parameters of the track. Generally, these should match the |audio_source| so
// that it does not have to perform unnecessary sample rate conversion or
// channel mixing.
CONTENT_EXPORT bool AddAudioTrackToMediaStream(
    scoped_refptr<media::AudioCapturerSource> audio_source,
    int sample_rate,
    media::ChannelLayout channel_layout,
    int frames_per_buffer,
    bool is_remote,
    blink::WebMediaStream* web_media_stream);

// Requests that a refresh frame be sent "soon" (e.g., to resolve picture loss
// or quality issues).
CONTENT_EXPORT void RequestRefreshFrameFromVideoTrack(
    const blink::WebMediaStreamTrack& video_track);

// Calls to these methods must be done on the main render thread.
// Note that |callback| for frame delivery happens on the IO thread.
// Warning: Calling RemoveSinkFromMediaStreamTrack does not immediately stop
// frame delivery through the |callback|, since frames are being delivered on
// a different thread.
// |is_sink_secure| indicates if |sink| meets output protection requirement.
// Generally, this should be false unless you know what you are doing.
CONTENT_EXPORT void AddSinkToMediaStreamTrack(
    const blink::WebMediaStreamTrack& track,
    blink::WebMediaStreamSink* sink,
    const blink::VideoCaptureDeliverFrameCB& callback,
    bool is_sink_secure);
CONTENT_EXPORT void RemoveSinkFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track,
    blink::WebMediaStreamSink* sink);

CONTENT_EXPORT void OnFrameDroppedAtMediaStreamSink(
    const blink::WebMediaStreamTrack& track,
    media::VideoCaptureFrameDropReason reason);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_UTILS_H_
