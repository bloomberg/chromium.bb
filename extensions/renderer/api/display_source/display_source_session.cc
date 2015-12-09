// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/display_source_session.h"

namespace extensions {

DisplaySourceSession::DisplaySourceSession()
  : state_(Idle) {
}

DisplaySourceSession::~DisplaySourceSession() {
}

void DisplaySourceSession::SetCallbacks(
    const SinkIdCallback& started_callback,
    const SinkIdCallback& terminated_callback,
    const ErrorCallback& error_callback) {
  DCHECK(started_callback_.is_null());
  DCHECK(terminated_callback_.is_null());
  DCHECK(error_callback_.is_null());

  started_callback_ = started_callback;
  terminated_callback_ = terminated_callback;
  error_callback_ = error_callback;
}

scoped_ptr<DisplaySourceSession> DisplaySourceSessionFactory::CreateSession(
    int sink_id,
    const blink::WebMediaStreamTrack& video_track,
    const blink::WebMediaStreamTrack& audio_track,
    scoped_ptr<DisplaySourceAuthInfo> auth_info) {
  return nullptr;
}

}  // namespace extensions
