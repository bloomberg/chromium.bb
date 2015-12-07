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
    blink::WebMediaSessionActivateCallback* raw_callback) {
  NOTIMPLEMENTED();

  scoped_ptr<blink::WebMediaSessionActivateCallback> callback(raw_callback);
  callback->onError(blink::WebMediaSessionError::Activate);
}

void WebMediaSessionAndroid::deactivate(
    blink::WebMediaSessionDeactivateCallback* raw_callback) {
  NOTIMPLEMENTED();

  scoped_ptr<blink::WebMediaSessionDeactivateCallback> callback(raw_callback);
  callback->onSuccess();
}

void WebMediaSessionAndroid::setMetadata(
    const blink::WebMediaMetadata* metadata) {
  NOTIMPLEMENTED();
}

}  // namespace content
