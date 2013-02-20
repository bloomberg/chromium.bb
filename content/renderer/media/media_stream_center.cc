// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSourcesRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/libjingle/source/talk/app/webrtc/jsep.h"

namespace content {

static webrtc::MediaStreamInterface* GetNativeMediaStream(
    const WebKit::WebMediaStream& stream) {
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  return extra_data->stream();
}


static webrtc::MediaStreamTrackInterface* GetNativeMediaStreamTrack(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaStreamTrack& component) {
  std::string track_id = UTF16ToUTF8(component.id());
  webrtc::MediaStreamInterface* native_stream = GetNativeMediaStream(stream);
  if (native_stream) {
    if (component.source().type() == WebKit::WebMediaStreamSource::TypeAudio) {
      return native_stream->FindAudioTrack(track_id);
    }
    if (component.source().type() == WebKit::WebMediaStreamSource::TypeVideo) {
      return native_stream->FindVideoTrack(track_id);
    }
  }
  NOTREACHED();
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
    const WebKit::WebMediaStream& stream,
    const WebKit::WebMediaStreamTrack& component) {
  webrtc::MediaStreamTrackInterface* track =
      GetNativeMediaStreamTrack(stream, component);
  if (track)
    track->set_enabled(true);
}

void MediaStreamCenter::didDisableMediaStreamTrack(
    const WebKit::WebMediaStream& stream,
    const WebKit::WebMediaStreamTrack& component) {
  webrtc::MediaStreamTrackInterface* track =
      GetNativeMediaStreamTrack(stream, component);
  if (track)
    track->set_enabled(false);
}

void MediaStreamCenter::didStopLocalMediaStream(
    const WebKit::WebMediaStream& stream) {
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
    WebKit::WebMediaStream& stream) {
  if (!rtc_factory_)
    return;
  rtc_factory_->CreateNativeLocalMediaStream(&stream);
}

}  // namespace content
