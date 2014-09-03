// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_source.h"

#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"

namespace content {

namespace {
// TODO(miu): This is a temporary hack until the Chrome audio vertical is
// migrated for the Cross-Site Isolation project.  http://crbug.com/392596
int ToRenderViewId(int render_frame_id) {
  RenderFrameImpl* const frame =
      RenderFrameImpl::FromRoutingID(render_frame_id);
  RenderViewImpl* const view = frame ? frame->render_view() : NULL;
  return view ? view->GetRoutingID() : -1;
}
}  // namespace

MediaStreamAudioSource::MediaStreamAudioSource(
    int render_frame_id,
    const StreamDeviceInfo& device_info,
    const SourceStoppedCallback& stop_callback,
    PeerConnectionDependencyFactory* factory)
    : render_view_id_(ToRenderViewId(render_frame_id)),
      factory_(factory) {
  SetDeviceInfo(device_info);
  SetStopCallback(stop_callback);
}

MediaStreamAudioSource::MediaStreamAudioSource()
    : render_view_id_(-1),
      factory_(NULL) {
}

MediaStreamAudioSource::~MediaStreamAudioSource() {}

void MediaStreamAudioSource::DoStopSource() {
  if (audio_capturer_.get())
    audio_capturer_->Stop();
}

void MediaStreamAudioSource::AddTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  // TODO(xians): Properly implement for audio sources.
  if (!local_audio_source_.get()) {
    if (!factory_->InitializeMediaStreamAudioSource(render_view_id_,
                                                    constraints,
                                                    this)) {
      // The source failed to start.
      // UserMediaClientImpl rely on the |stop_callback| to be triggered when
      // the last track is removed from the source. But in this case, the
      // source is is not even started. So we need to fail both adding the
      // track and trigger |stop_callback|.
      callback.Run(this, MEDIA_DEVICE_TRACK_START_FAILURE, "");
      StopSource();
      return;
    }
  }

  factory_->CreateLocalAudioTrack(track);
  callback.Run(this, MEDIA_DEVICE_OK, "");
}

}  // namespace content
