// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_utils.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/video_capturer_source.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"

namespace content {

bool AddVideoTrackToMediaStream(
    std::unique_ptr<media::VideoCapturerSource> video_source,
    bool is_remote,
    bool is_readonly,
    blink::WebMediaStream* web_media_stream) {
  DCHECK(video_source.get());
  if (!web_media_stream || web_media_stream->isNull()) {
    DLOG(ERROR) << "WebMediaStream is null";
    return false;
  }

  blink::WebMediaStreamSource web_media_stream_source;
  MediaStreamVideoSource* const media_stream_source =
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(), std::move(video_source));
  const blink::WebString track_id =
      blink::WebString::fromUTF8(base::GenerateGUID());
  web_media_stream_source.initialize(track_id,
                                     blink::WebMediaStreamSource::TypeVideo,
                                     track_id, is_remote);
  // Takes ownership of |media_stream_source|.
  web_media_stream_source.setExtraData(media_stream_source);

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  web_media_stream->addTrack(MediaStreamVideoTrack::CreateVideoTrack(
      media_stream_source, constraints,
      MediaStreamVideoSource::ConstraintsCallback(), true));
  return true;
}

bool AddAudioTrackToMediaStream(
    scoped_refptr<media::AudioCapturerSource> audio_source,
    int sample_rate,
    media::ChannelLayout channel_layout,
    int frames_per_buffer,
    bool is_remote,
    bool is_readonly,
    blink::WebMediaStream* web_media_stream) {
  DCHECK(audio_source.get());
  if (!web_media_stream || web_media_stream->isNull()) {
    DLOG(ERROR) << "WebMediaStream is null";
    return false;
  }

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      sample_rate, sizeof(int16_t) * 8, frames_per_buffer);
  if (!params.IsValid()) {
    DLOG(ERROR) << "Invalid audio parameters.";
    return false;
  }

  blink::WebMediaStreamSource web_media_stream_source;
  const blink::WebString track_id =
      blink::WebString::fromUTF8(base::GenerateGUID());
  web_media_stream_source.initialize(track_id,
                                     blink::WebMediaStreamSource::TypeAudio,
                                     track_id, is_remote);

  MediaStreamAudioSource* media_stream_source(new MediaStreamAudioSource(
      -1, StreamDeviceInfo(), MediaStreamSource::SourceStoppedCallback(),
      RenderThreadImpl::current()->GetPeerConnectionDependencyFactory()));

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  {
    // TODO(miu): In an upcoming change, a source purposed for passing audio
    // directly (i.e., without modification) will replace this "hacky" use of
    // WebRtcAudioCapturer.  http://crbug.com/577881
    std::unique_ptr<WebRtcAudioCapturer> capturer(
        WebRtcAudioCapturer::CreateCapturer(-1, StreamDeviceInfo(), constraints,
                                            nullptr, media_stream_source));
    capturer->SetCapturerSource(std::move(audio_source), params);
    media_stream_source->SetAudioCapturer(std::move(capturer));
  }
  web_media_stream_source.setExtraData(
      media_stream_source);  // Takes ownership.

  blink::WebMediaStreamTrack web_media_stream_track;
  web_media_stream_track.initialize(web_media_stream_source);
  RenderThreadImpl::current()
      ->GetPeerConnectionDependencyFactory()
      ->CreateLocalAudioTrack(web_media_stream_track);

  web_media_stream->addTrack(web_media_stream_track);
  return true;
}

const media::VideoCaptureFormat* GetCurrentVideoTrackFormat(
    const blink::WebMediaStreamTrack& video_track) {
  if (video_track.isNull())
    return nullptr;

  MediaStreamVideoSource* const source =
      MediaStreamVideoSource::GetVideoSource(video_track.source());
  if (!source)
    return nullptr;

  return source->GetCurrentFormat();
}

void RequestRefreshFrameFromVideoTrack(
    const blink::WebMediaStreamTrack& video_track) {
  if (video_track.isNull())
    return;
  MediaStreamVideoSource* const source =
      MediaStreamVideoSource::GetVideoSource(video_track.source());
  if (source)
    source->RequestRefreshFrame();
}

}  // namespace content
