// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSourcesRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/libjingle/source/talk/app/webrtc/jsep.h"

namespace content {

static webrtc::MediaStreamInterface* GetNativeMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  if (extra_data && extra_data->remote_stream())
    return extra_data->remote_stream();

  if (extra_data && extra_data->local_stream())
    return extra_data->local_stream();

  // TODO(perkj): This can occur if a JS create a new MediaStream based on an
  // existing MediaStream.
  NOTIMPLEMENTED();
  return NULL;
}

template <class TrackList>
static webrtc::MediaStreamTrackInterface* GetTrack(
    const std::string& source_id,
    TrackList* tracks) {
  for (size_t i = 0; i < tracks->count(); ++i) {
    if (tracks->at(i)->id() == source_id)
      return tracks->at(i);
  }
  return NULL;
}

static webrtc::MediaStreamTrackInterface* GetNativeMediaStreamTrack(
      const WebKit::WebMediaStreamDescriptor& stream,
      const WebKit::WebMediaStreamComponent& component) {
  std::string source_id = UTF16ToUTF8(component.source().id());
  webrtc::MediaStreamInterface* native_stream = GetNativeMediaStream(stream);
  if (native_stream) {
    if (component.source().type() == WebKit::WebMediaStreamSource::TypeAudio) {
      return GetTrack<webrtc::AudioTracks>(
          source_id, native_stream->audio_tracks());
    }
    if (component.source().type() == WebKit::WebMediaStreamSource::TypeVideo) {
      return GetTrack<webrtc::VideoTracks>(
          source_id, native_stream->video_tracks());
    }
  }
  // TODO(perkj): This can occur if a JS create a new MediaStream based on an
  // existing MediaStream.
  NOTIMPLEMENTED();
  return NULL;
}

MediaStreamCenter::MediaStreamCenter(WebKit::WebMediaStreamCenterClient* client,
                                     MediaStreamDependencyFactory* factory)
    : rtc_factory_(factory) {
}

void MediaStreamCenter::queryMediaStreamSources(
    const WebKit::WebMediaStreamSourcesRequest& request) {
  WebKit::WebVector<WebKit::WebMediaStreamSource> audioSources, videoSources;
  request.didCompleteQuery(audioSources, videoSources);
}

void MediaStreamCenter::didEnableMediaStreamTrack(
    const WebKit::WebMediaStreamDescriptor& stream,
    const WebKit::WebMediaStreamComponent& component) {
  webrtc::MediaStreamTrackInterface* track =
      GetNativeMediaStreamTrack(stream, component);
  if (track)
    track->set_enabled(true);
}

void MediaStreamCenter::didDisableMediaStreamTrack(
    const WebKit::WebMediaStreamDescriptor& stream,
    const WebKit::WebMediaStreamComponent& component) {
  webrtc::MediaStreamTrackInterface* track =
      GetNativeMediaStreamTrack(stream, component);
  if (track)
    track->set_enabled(false);
}

void MediaStreamCenter::didStopLocalMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  DVLOG(1) << "MediaStreamCenter::didStopLocalMediaStream";
  MediaStreamExtraData* extra_data =
       static_cast<MediaStreamExtraData*>(stream.extraData());
  if (!extra_data) {
    NOTREACHED();
    return;
  }

  extra_data->OnLocalStreamStop();
}

void MediaStreamCenter::didCreateMediaStream(
    WebKit::WebMediaStreamDescriptor& stream) {
  if (!rtc_factory_)
    return;
  rtc_factory_->CreateNativeLocalMediaStream(&stream);
}

}  // namespace content
