// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_source_observer.h"

#include "base/logging.h"

namespace content {

MediaStreamSourceObserver::MediaStreamSourceObserver(
    webrtc::MediaSourceInterface* webrtc_source,
    const WebKit::WebMediaStreamSource& webkit_source)
     : state_(webrtc_source->state()),
       webrtc_source_(webrtc_source),
       webkit_source_(webkit_source) {
  DCHECK(CalledOnValidThread());
  webrtc_source_->RegisterObserver(this);
}

MediaStreamSourceObserver::~MediaStreamSourceObserver() {
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

  switch (state) {
    case webrtc::MediaSourceInterface::kInitializing:
      /* kInitializing is not ready state. */
      break;
    case webrtc::MediaSourceInterface::kLive:
      webkit_source_.setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateLive);
      break;
    case webrtc::MediaSourceInterface::kMuted:
      webkit_source_.setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateMuted);
      break;
    case webrtc::MediaSourceInterface::kEnded:
      webkit_source_.setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateEnded);
      webrtc_source_->UnregisterObserver(this);
      webrtc_source_ = NULL;
      webkit_source_.reset();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace content
