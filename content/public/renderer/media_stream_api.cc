// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_api.h"

#include <utility>

#include "base/base64.h"
#include "base/callback.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

namespace content {

namespace {

blink::WebString MakeTrackId() {
  std::string track_id;
  base::Base64Encode(base::RandBytesAsString(64), &track_id);
  return base::UTF8ToUTF16(track_id);
}

}  // namespace

bool AddVideoTrackToMediaStream(scoped_ptr<media::VideoCapturerSource> source,
                                bool is_remote,
                                bool is_readonly,
                                const std::string& media_stream_url) {
  blink::WebMediaStream web_stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(
          GURL(media_stream_url));
  return AddVideoTrackToMediaStream(std::move(source), is_remote, is_readonly,
                                    &web_stream);
}

bool AddVideoTrackToMediaStream(scoped_ptr<media::VideoCapturerSource> source,
                                bool is_remote,
                                bool is_readonly,
                                blink::WebMediaStream* web_stream) {
  if (web_stream->isNull()) {
    DLOG(ERROR) << "Stream not found";
    return false;
  }
  const blink::WebString track_id = MakeTrackId();
  blink::WebMediaStreamSource webkit_source;
  scoped_ptr<MediaStreamVideoSource> media_stream_source(
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(), std::move(source)));
  webkit_source.initialize(track_id, blink::WebMediaStreamSource::TypeVideo,
                           track_id, is_remote, is_readonly);
  webkit_source.setExtraData(media_stream_source.get());

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  web_stream->addTrack(MediaStreamVideoTrack::CreateVideoTrack(
      media_stream_source.release(), constraints,
      MediaStreamVideoSource::ConstraintsCallback(), true));
  return true;
}

bool AddAudioTrackToMediaStream(
    const scoped_refptr<media::AudioCapturerSource>& source,
    const media::AudioParameters& params,
    bool is_remote,
    bool is_readonly,
    const std::string& media_stream_url) {
  DCHECK(params.IsValid()) << params.AsHumanReadableString();
  blink::WebMediaStream web_stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(
          GURL(media_stream_url));
  return AddAudioTrackToMediaStream(source,
                                    is_remote, is_readonly, &web_stream);
}

bool AddAudioTrackToMediaStream(
    const scoped_refptr<media::AudioCapturerSource>& source,
    bool is_remote,
    bool is_readonly,
    blink::WebMediaStream* web_stream) {
  if (web_stream->isNull()) {
    DLOG(ERROR) << "Stream not found";
    return false;
  }

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LINEAR, media::CHANNEL_LAYOUT_STEREO,
      48000, /* sample rate */
      16,    /* bits per sample */
      480);  /* frames per buffer */

  blink::WebMediaStreamSource webkit_source;
  const blink::WebString track_id = MakeTrackId();
  webkit_source.initialize(track_id,
                           blink::WebMediaStreamSource::TypeAudio,
                           track_id,
                           is_remote,
                           is_readonly);

  MediaStreamAudioSource* audio_source(
      new MediaStreamAudioSource(
          -1,
          StreamDeviceInfo(),
          MediaStreamSource::SourceStoppedCallback(),
          RenderThreadImpl::current()->GetPeerConnectionDependencyFactory()));

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  scoped_refptr<WebRtcAudioCapturer> capturer(
      WebRtcAudioCapturer::CreateCapturer(-1, StreamDeviceInfo(), constraints,
                                          nullptr, audio_source));
  capturer->SetCapturerSource(source, params);
  audio_source->SetAudioCapturer(capturer);
  webkit_source.setExtraData(audio_source);

  blink::WebMediaStreamTrack web_media_audio_track;
  web_media_audio_track.initialize(webkit_source);
  RenderThreadImpl::current()->GetPeerConnectionDependencyFactory()->
      CreateLocalAudioTrack(web_media_audio_track);

  web_stream->addTrack(web_media_audio_track);
  return true;
}

}  // namespace content
