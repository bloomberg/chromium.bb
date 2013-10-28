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
#include "content/renderer/media/media_stream_source_extra_data.h"
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
      MediaStreamDependencyFactory::GetNativeMediaStreamTrack(component);
  if (track)
    track->set_enabled(true);
}

void MediaStreamCenter::didDisableMediaStreamTrack(
    const WebKit::WebMediaStream& stream,
    const WebKit::WebMediaStreamTrack& component) {
  webrtc::MediaStreamTrackInterface* track =
      MediaStreamDependencyFactory::GetNativeMediaStreamTrack(component);
  if (track)
    track->set_enabled(false);
}

bool MediaStreamCenter::didStopMediaStreamTrack(
    const WebKit::WebMediaStreamTrack& web_track) {
  DVLOG(1) << "MediaStreamCenter::didStopMediaStreamTrack";
  WebKit::WebMediaStreamSource web_source = web_track.source();
  MediaStreamSourceExtraData* extra_data =
      static_cast<MediaStreamSourceExtraData*>(web_source.extraData());
  if (!extra_data) {
    DVLOG(1) << "didStopMediaStreamTrack called on a remote track.";
    return false;
  }

  extra_data->OnLocalSourceStop();
  return true;
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

  // TODO(perkj): MediaStream::Stop is being deprecated. But for the moment we
  // need to support the old behavior and the new. Since we only create one
  // source object per actual device- we need to fake stopping a
  // MediaStreamTrack by disabling it if the same device is used as source by
  // multiple tracks. Note that disabling a track here, don't affect the
  // enabled property in JS.
  WebKit::WebVector<WebKit::WebMediaStreamTrack> audio_tracks;
  stream.audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i)
    didDisableMediaStreamTrack(stream, audio_tracks[i]);

  WebKit::WebVector<WebKit::WebMediaStreamTrack> video_tracks;
  stream.videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i)
    didDisableMediaStreamTrack(stream, video_tracks[i]);

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
    const MediaStreamDevice& device = devices[i].device;
    DCHECK(device.type == MEDIA_DEVICE_AUDIO_CAPTURE ||
           device.type == MEDIA_DEVICE_VIDEO_CAPTURE);
    WebKit::WebSourceInfo::VideoFacingMode video_facing;
    switch (device.video_facing) {
      case MEDIA_VIDEO_FACING_USER:
        video_facing = WebKit::WebSourceInfo::VideoFacingModeUser;
        break;
      case MEDIA_VIDEO_FACING_ENVIRONMENT:
        video_facing = WebKit::WebSourceInfo::VideoFacingModeEnvironment;
        break;
      default:
        video_facing = WebKit::WebSourceInfo::VideoFacingModeNone;
    }

    sourceInfos[i]
        .initialize(WebKit::WebString::fromUTF8(device.id),
                    device.type == MEDIA_DEVICE_AUDIO_CAPTURE
                        ? WebKit::WebSourceInfo::SourceKindAudio
                        : WebKit::WebSourceInfo::SourceKindVideo,
                    WebKit::WebString::fromUTF8(device.name),
                    video_facing);
  }
  request_it->second.requestSucceeded(sourceInfos);
}

}  // namespace content
