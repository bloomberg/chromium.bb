// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_renderer_factory_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/media_stream_video_renderer_sink.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/media/webrtc_local_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_hardware_config.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

namespace {

PeerConnectionDependencyFactory* GetPeerConnectionDependencyFactory() {
  return RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
}

// Returns a valid session id if a single capture device is currently open
// (and then the matching session_id), otherwise -1.
// This is used to pass on a session id to a webrtc audio renderer (either
// local or remote), so that audio will be rendered to a matching output
// device, should one exist.
// Note that if there are more than one open capture devices the function
// will not be able to pick an appropriate device and return false.
bool GetSessionIdForAudioRenderer(int* session_id) {
  WebRtcAudioDeviceImpl* audio_device =
      GetPeerConnectionDependencyFactory()->GetWebRtcAudioDevice();
  if (!audio_device)
    return false;

  int sample_rate;        // ignored, read from output device
  int frames_per_buffer;  // ignored, read from output device
  return audio_device->GetAuthorizedDeviceInfoForAudioRenderer(
      session_id, &sample_rate, &frames_per_buffer);
}

scoped_refptr<WebRtcAudioRenderer> CreateRemoteAudioRenderer(
    const blink::WebMediaStream& stream,
    int render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  DVLOG(1) << "MediaStreamRendererFactoryImpl::CreateRemoteAudioRenderer id:"
           << stream.id().utf8();
  // |stream| will always contain at least one audio track.
  // See MediaStreamRendererFactoryImpl::GetAudioRenderer.

  // TODO(tommi): Change the default value of session_id to be
  // StreamDeviceInfo::kNoId.  Also update AudioOutputDevice etc.
  int session_id = 0;
  GetSessionIdForAudioRenderer(&session_id);

  return new WebRtcAudioRenderer(
      GetPeerConnectionDependencyFactory()->GetWebRtcSignalingThread(), stream,
      render_frame_id, session_id, device_id, security_origin);
}

scoped_refptr<WebRtcLocalAudioRenderer> CreateLocalAudioRenderer(
    const blink::WebMediaStreamTrack& audio_track,
    int render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  DVLOG(1) << "MediaStreamRendererFactoryImpl::CreateLocalAudioRenderer";

  int session_id = 0;
  GetSessionIdForAudioRenderer(&session_id);

  // Create a new WebRtcLocalAudioRenderer instance and connect it to the
  // existing WebRtcAudioCapturer so that the renderer can use it as source.
  return new WebRtcLocalAudioRenderer(audio_track, render_frame_id, session_id,
                                      device_id, security_origin);
}

}  // namespace


MediaStreamRendererFactoryImpl::MediaStreamRendererFactoryImpl() {
}

MediaStreamRendererFactoryImpl::~MediaStreamRendererFactoryImpl() {
}

scoped_refptr<VideoFrameProvider>
MediaStreamRendererFactoryImpl::GetVideoFrameProvider(
    const GURL& url,
    const base::Closure& error_cb,
    const VideoFrameProvider::RepaintCB& repaint_cb,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  blink::WebMediaStream web_stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url);
  DCHECK(!web_stream.isNull());

  DVLOG(1) << "MediaStreamRendererFactoryImpl::GetVideoFrameProvider stream:"
           << base::UTF16ToUTF8(base::StringPiece16(web_stream.id()));

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream.videoTracks(video_tracks);
  if (video_tracks.isEmpty() ||
      !MediaStreamVideoTrack::GetTrack(video_tracks[0])) {
    return NULL;
  }

  return new MediaStreamVideoRendererSink(video_tracks[0], error_cb, repaint_cb,
                                          media_task_runner, worker_task_runner,
                                          gpu_factories);
}

scoped_refptr<MediaStreamAudioRenderer>
MediaStreamRendererFactoryImpl::GetAudioRenderer(
    const GURL& url,
    int render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  blink::WebMediaStream web_stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url);

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream.audioTracks(audio_tracks);
  if (audio_tracks.isEmpty())
    return NULL;

  DVLOG(1) << "MediaStreamRendererFactoryImpl::GetAudioRenderer stream:"
           << base::UTF16ToUTF8(base::StringPiece16(web_stream.id()));

  // TODO(tommi): We need to fix the data flow so that
  // it works the same way for all track implementations, local, remote or what
  // have you.
  // In this function, we should simply create a renderer object that receives
  // and mixes audio from all the tracks that belong to the media stream.
  // For now, we have separate renderers depending on if the first audio track
  // in the stream is local or remote.
  MediaStreamAudioTrack* audio_track = MediaStreamAudioTrack::GetTrack(
      audio_tracks[0]);
  if (!audio_track) {
    // This can happen if the track was cloned.
    // TODO(tommi, perkj): Fix cloning of tracks to handle extra data too.
    LOG(ERROR) << "No native track for WebMediaStreamTrack.";
    return nullptr;
  }

  if (audio_track->is_local_track()) {
    // TODO(xians): Add support for the case where the media stream contains
    // multiple audio tracks.
    return CreateLocalAudioRenderer(audio_tracks[0], render_frame_id, device_id,
                                    security_origin);
  }

  // This is a remote WebRTC media stream.
  WebRtcAudioDeviceImpl* audio_device =
      GetPeerConnectionDependencyFactory()->GetWebRtcAudioDevice();

  // Share the existing renderer if any, otherwise create a new one.
  scoped_refptr<WebRtcAudioRenderer> renderer(audio_device->renderer());
  if (!renderer.get()) {
    renderer = CreateRemoteAudioRenderer(web_stream, render_frame_id,
                                         device_id, security_origin);

    if (renderer.get() && !audio_device->SetAudioRenderer(renderer.get()))
      renderer = NULL;
  }

  return renderer.get() ? renderer->CreateSharedAudioRendererProxy(web_stream)
                        : NULL;
}

}  // namespace content
