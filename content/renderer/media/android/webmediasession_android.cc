// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediasession_android.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/android/renderer_media_session_manager.h"

namespace content {

WebMediaSessionAndroid::WebMediaSessionAndroid(
    RendererMediaSessionManager* session_manager)
    : session_manager_(session_manager) {
  DCHECK(session_manager_);
  session_id_ = session_manager_->RegisterMediaSession(this);
}

WebMediaSessionAndroid::~WebMediaSessionAndroid() {
  session_manager_->UnregisterMediaSession(session_id_);
}

void WebMediaSessionAndroid::activate(
    blink::WebMediaSessionActivateCallback* callback) {
  session_manager_->Activate(session_id_, make_scoped_ptr(callback));
}

void WebMediaSessionAndroid::deactivate(
    blink::WebMediaSessionDeactivateCallback* callback) {
  session_manager_->Deactivate(session_id_, make_scoped_ptr(callback));
}

void WebMediaSessionAndroid::setMetadata(
    const blink::WebMediaMetadata* metadata) {
  NOTIMPLEMENTED();
}

}  // namespace content
