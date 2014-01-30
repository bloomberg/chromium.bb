// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_source.h"

namespace content {

MediaStreamSource::MediaStreamSource() {
}

MediaStreamSource::~MediaStreamSource() {}

void MediaStreamSource::StopSource() {
  DoStopSource();
  if (!stop_callback_.is_null())
    stop_callback_.Run(owner());

  owner().setReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
  owner().setExtraData(NULL);
}

}  // namespace content
