// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/renderer_media_session_manager.h"

#include "base/logging.h"
#include "content/renderer/media/android/webmediasession_android.h"

namespace content {

static const int kDefaultMediaSessionID = 0;

RendererMediaSessionManager::RendererMediaSessionManager(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      next_session_id_(kDefaultMediaSessionID + 1) {}

RendererMediaSessionManager::~RendererMediaSessionManager() {
  DCHECK(sessions_.empty())
      << "RendererMediaSessionManager is owned by RenderFrameImpl and is "
         "destroyed only after all media sessions are destroyed.";
}

int RendererMediaSessionManager::RegisterMediaSession(
    WebMediaSessionAndroid* session) {
  sessions_[next_session_id_] = session;
  return next_session_id_++;
}

void RendererMediaSessionManager::UnregisterMediaSession(int session_id) {
  sessions_.erase(session_id);
}

}  // namespace content
