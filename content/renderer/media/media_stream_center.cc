// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/libjingle/source/talk/app/webrtc/jsep.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICECandidateDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSourcesRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSessionDescriptionDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

namespace content {

static MediaStreamImpl* GetMediaStreamImpl(WebKit::WebFrame* web_frame) {
  RenderViewImpl* render_view = RenderViewImpl::FromWebView(web_frame->view());
  if (!render_view)
    return NULL;

  // TODO(perkj): Avoid this cast?
  return static_cast<MediaStreamImpl*>(render_view->userMediaClient());
}

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
    if (tracks->at(i)->label() == source_id)
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

MediaStreamCenter::MediaStreamCenter(
    WebKit::WebMediaStreamCenterClient* client)
    : client_(client) {
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
  WebKit::WebFrame* web_frame = WebKit::WebFrame::frameForCurrentContext();
  if (!web_frame)
    return;
  MediaStreamImpl* ms_impl = GetMediaStreamImpl(web_frame);
  if (ms_impl) {
    ms_impl->StopLocalMediaStream(stream);
    return;
  }

  NOTREACHED();
}

void MediaStreamCenter::didCreateMediaStream(
    WebKit::WebMediaStreamDescriptor& stream) {
  WebKit::WebFrame* web_frame = WebKit::WebFrame::frameForCurrentContext();
  if (!web_frame)
    return;
  MediaStreamImpl* ms_impl = GetMediaStreamImpl(web_frame);
  if (ms_impl) {
    ms_impl->CreateMediaStream(web_frame, &stream);
    return;
  }
  NOTREACHED();
}

WebKit::WebString MediaStreamCenter::constructSDP(
    const WebKit::WebICECandidateDescriptor& candidate) {
  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      webrtc::CreateIceCandidate(UTF16ToUTF8(candidate.label()),
                                 UTF16ToUTF8(candidate.candidateLine())));
  std::string sdp;
  if (!native_candidate->ToString(&sdp))
    LOG(ERROR) << "Could not create SDP string";
  return UTF8ToUTF16(sdp);
}

WebKit::WebString MediaStreamCenter::constructSDP(
    const WebKit::WebSessionDescriptionDescriptor& description) {
  scoped_ptr<webrtc::SessionDescriptionInterface> native_desc(
      webrtc::CreateSessionDescription(UTF16ToUTF8(description.initialSDP())));
  if (!native_desc.get())
    return WebKit::WebString();

  for (size_t i = 0; i < description.numberOfAddedCandidates(); ++i) {
    WebKit::WebICECandidateDescriptor candidate = description.candidate(i);
    scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
        webrtc::CreateIceCandidate(UTF16ToUTF8(candidate.label()),
                                   UTF16ToUTF8(candidate.candidateLine())));
    native_desc->AddCandidate(native_candidate.get());
  }

  std::string sdp;
  if (!native_desc->ToString(&sdp))
    LOG(ERROR) << "Could not create SDP string";
  return UTF8ToUTF16(sdp);
}

}  // namespace content
