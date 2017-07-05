// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webaudio_media_stream_source.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebFrame;
using blink::WebView;

namespace content {

namespace {

void CreateNativeAudioMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.Source();
  MediaStreamAudioSource* media_stream_source =
      MediaStreamAudioSource::From(source);

  // At this point, a MediaStreamAudioSource instance must exist. The one
  // exception is when a WebAudio destination node is acting as a source of
  // audio.
  //
  // TODO(miu): This needs to be moved to an appropriate location. A WebAudio
  // source should have been created before this method was called so that this
  // special case code isn't needed here.
  if (!media_stream_source && source.RequiresAudioConsumer()) {
    DVLOG(1) << "Creating WebAudio media stream source.";
    media_stream_source = new WebAudioMediaStreamSource(&source);
    source.SetExtraData(media_stream_source);  // Takes ownership.
  }

  if (media_stream_source)
    media_stream_source->ConnectToTrack(track);
  else
    LOG(DFATAL) << "WebMediaStreamSource missing its MediaStreamAudioSource.";
}

void CreateNativeVideoMediaStreamTrack(blink::WebMediaStreamTrack track) {
  DCHECK(track.GetTrackData() == NULL);
  blink::WebMediaStreamSource source = track.Source();
  DCHECK_EQ(source.GetType(), blink::WebMediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  track.SetTrackData(new MediaStreamVideoTrack(
      native_source, MediaStreamVideoSource::ConstraintsCallback(),
      track.IsEnabled()));
}

void CloneNativeVideoMediaStreamTrack(
    const blink::WebMediaStreamTrack& original,
    blink::WebMediaStreamTrack clone) {
  DCHECK(!clone.GetTrackData());
  blink::WebMediaStreamSource source = clone.Source();
  DCHECK_EQ(source.GetType(), blink::WebMediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  MediaStreamVideoTrack* original_track =
      MediaStreamVideoTrack::GetVideoTrack(original);
  DCHECK(original_track);
  clone.SetTrackData(new MediaStreamVideoTrack(
      native_source, original_track->adapter_settings(),
      original_track->noise_reduction(), original_track->is_screencast(),
      original_track->min_frame_rate(),
      MediaStreamVideoSource::ConstraintsCallback(), clone.IsEnabled()));
}

}  // namespace

MediaStreamCenter::MediaStreamCenter(
    blink::WebMediaStreamCenterClient* client,
    PeerConnectionDependencyFactory* factory) {}

MediaStreamCenter::~MediaStreamCenter() {}

void MediaStreamCenter::DidCreateMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didCreateMediaStreamTrack";
  DCHECK(!track.IsNull() && !track.GetTrackData());
  DCHECK(!track.Source().IsNull());

  switch (track.Source().GetType()) {
    case blink::WebMediaStreamSource::kTypeAudio:
      CreateNativeAudioMediaStreamTrack(track);
      break;
    case blink::WebMediaStreamSource::kTypeVideo:
      CreateNativeVideoMediaStreamTrack(track);
      break;
  }
}

void MediaStreamCenter::DidCloneMediaStreamTrack(
    const blink::WebMediaStreamTrack& original,
    const blink::WebMediaStreamTrack& clone) {
  DCHECK(!clone.IsNull());
  DCHECK(!clone.GetTrackData());
  DCHECK(!clone.Source().IsNull());

  switch (clone.Source().GetType()) {
    case blink::WebMediaStreamSource::kTypeAudio:
      CreateNativeAudioMediaStreamTrack(clone);
      break;
    case blink::WebMediaStreamSource::kTypeVideo:
      CloneNativeVideoMediaStreamTrack(original, clone);
      break;
  }
}

void MediaStreamCenter::DidSetContentHint(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetContentHint(track.ContentHint());
}

void MediaStreamCenter::DidEnableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track =
      MediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(true);
}

void MediaStreamCenter::DidDisableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track =
      MediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(false);
}

bool MediaStreamCenter::DidStopMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didStopMediaStreamTrack";
  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  native_track->Stop();
  return true;
}

blink::WebAudioSourceProvider*
MediaStreamCenter::CreateWebAudioSourceFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::createWebAudioSourceFromMediaStreamTrack";
  MediaStreamTrack* media_stream_track =
      static_cast<MediaStreamTrack*>(track.GetTrackData());
  if (!media_stream_track) {
    DLOG(ERROR) << "Native track missing for webaudio source.";
    return nullptr;
  }

  blink::WebMediaStreamSource source = track.Source();
  DCHECK_EQ(source.GetType(), blink::WebMediaStreamSource::kTypeAudio);

  // TODO(tommi): Rename WebRtcLocalAudioSourceProvider to
  // WebAudioMediaStreamSink since it's not specific to any particular source.
  // http://crbug.com/577874
  return new WebRtcLocalAudioSourceProvider(track);
}

void MediaStreamCenter::DidStopLocalMediaStream(
    const blink::WebMediaStream& stream) {
  DVLOG(1) << "MediaStreamCenter::didStopLocalMediaStream";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  if (!native_stream) {
    NOTREACHED();
    return;
  }

  // TODO(perkj): MediaStream::Stop is being deprecated. But for the moment we
  // need to support both MediaStream::Stop and MediaStreamTrack::Stop.
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  stream.AudioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i)
    DidStopMediaStreamTrack(audio_tracks[i]);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  stream.VideoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i)
    DidStopMediaStreamTrack(video_tracks[i]);
}

void MediaStreamCenter::DidCreateMediaStream(blink::WebMediaStream& stream) {
  DVLOG(1) << "MediaStreamCenter::didCreateMediaStream";
  blink::WebMediaStream writable_stream(stream);
  MediaStream* native_stream(new MediaStream());
  writable_stream.SetExtraData(native_stream);
}

bool MediaStreamCenter::DidAddMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didAddMediaStreamTrack";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  return native_stream->AddTrack(track);
}

bool MediaStreamCenter::DidRemoveMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didRemoveMediaStreamTrack";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  return native_stream->RemoveTrack(track);
}

}  // namespace content
