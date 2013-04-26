// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_source_observer.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_source_extra_data.h"

namespace content {

MediaStreamSourceObserver::MediaStreamSourceObserver(
    webrtc::MediaSourceInterface* webrtc_source,
    MediaStreamSourceExtraData* extra_data)
     : state_(webrtc_source->state()),
       webrtc_source_(webrtc_source),
       extra_data_(extra_data) {
  webrtc_source_->RegisterObserver(this);
}

MediaStreamSourceObserver::~MediaStreamSourceObserver() {
  DCHECK(CalledOnValidThread());
  if (webrtc_source_)
    webrtc_source_->UnregisterObserver(this);
}

void MediaStreamSourceObserver::OnChanged() {
  DCHECK(CalledOnValidThread());
  // There should be no more notification after kEnded.
  DCHECK(webrtc_source_ != NULL);

  webrtc::MediaSourceInterface::SourceState state = webrtc_source_->state();
  if (state == state_)
    return;
  state_ = state;
  WebKit::WebMediaStreamSource webkit_source(extra_data_->webkit_source());

  switch (state) {
    case webrtc::MediaSourceInterface::kInitializing:
      // Ignore the kInitializing state since there is no match in
      // WebMediaStreamSource::ReadyState.
      break;
    case webrtc::MediaSourceInterface::kLive:
      webkit_source.setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateLive);
      break;
    case webrtc::MediaSourceInterface::kMuted:
      webkit_source.setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateMuted);
      break;
    case webrtc::MediaSourceInterface::kEnded:
      webkit_source.setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateEnded);
      webrtc_source_->UnregisterObserver(this);
      webrtc_source_ = NULL;
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
