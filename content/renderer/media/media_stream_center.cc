// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/libjingle/source/talk/app/webrtc/jsep.h"

using WebKit::WebFrame;
using WebKit::WebView;

namespace content {

static webrtc::MediaStreamInterface* GetNativeMediaStream(
    const WebKit::WebMediaStream& stream) {
  MediaStreamExtraData* extra_data =
      static_cast<MediaStreamExtraData*>(stream.extraData());
  return extra_data->stream().get();
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
    : rtc_factory_(factory), next_request_id_(0) {}

MediaStreamCenter::~MediaStreamCenter() {}

bool MediaStreamCenter::getMediaStreamTrackSources(
    const WebKit::WebMediaStreamTrackSourcesRequest& request) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDeviceEnumeration)) {
    int request_id = next_request_id_++;
    requests_.insert(std::make_pair(request_id, request));
    RenderThread::Get()->Send(new MediaStreamHostMsg_GetSources(
        request_id, GURL(request.origin().utf8())));
    return true;
  }
  return false;
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

bool MediaStreamCenter::didAddMediaStreamTrack(
    const WebKit::WebMediaStream& stream,
    const WebKit::WebMediaStreamTrack& track) {
  if (!rtc_factory_)
    return false;

  return rtc_factory_->AddNativeMediaStreamTrack(stream, track);
}

bool MediaStreamCenter::didRemoveMediaStreamTrack(
    const WebKit::WebMediaStream& stream,
    const WebKit::WebMediaStreamTrack& track) {
  if (!rtc_factory_)
    return false;

  return rtc_factory_->RemoveNativeMediaStreamTrack(stream, track);
}

bool MediaStreamCenter::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamCenter, message)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_GetSourcesACK,
                        OnGetSourcesComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamCenter::OnGetSourcesComplete(
    int request_id,
    const content::StreamDeviceInfoArray& devices) {
  RequestMap::iterator request_it = requests_.find(request_id);
  DCHECK(request_it != requests_.end());

  WebKit::WebVector<WebKit::WebSourceInfo> sourceInfos(devices.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    DCHECK(devices[i].device.type == MEDIA_DEVICE_AUDIO_CAPTURE ||
           devices[i].device.type == MEDIA_DEVICE_VIDEO_CAPTURE);
    // TODO(vrk): Hook this up to the device policy so that |device.name| is
    // only populated when appropriate.
    sourceInfos[i]
        .initialize(WebKit::WebString::fromUTF8(devices[i].device.id),
                    devices[i].device.type == MEDIA_DEVICE_AUDIO_CAPTURE
                        ? WebKit::WebSourceInfo::SourceKindAudio
                        : WebKit::WebSourceInfo::SourceKindVideo,
                    WebKit::WebString::fromUTF8(devices[i].device.name),
                    WebKit::WebSourceInfo::VideoFacingModeNone);
  }
  request_it->second.requestSucceeded(sourceInfos);
}

}  // namespace content
