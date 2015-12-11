// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/renderer_media_session_manager.h"

#include "base/logging.h"
#include "content/common/media/media_session_messages_android.h"
#include "content/public/renderer/render_thread.h"
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

bool RendererMediaSessionManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererMediaSessionManager, msg)
    IPC_MESSAGE_HANDLER(MediaSessionMsg_DidActivate, OnDidActivate)
    IPC_MESSAGE_HANDLER(MediaSessionMsg_DidDeactivate, OnDidDeactivate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

int RendererMediaSessionManager::RegisterMediaSession(
    WebMediaSessionAndroid* session) {
  sessions_[next_session_id_] = session;
  return next_session_id_++;
}

void RendererMediaSessionManager::UnregisterMediaSession(int session_id) {
  sessions_.erase(session_id);
}

void RendererMediaSessionManager::Activate(
    int session_id,
    scoped_ptr<blink::WebMediaSessionActivateCallback> callback) {
  int request_id = pending_activation_requests_.Add(callback.release());
  Send(new MediaSessionHostMsg_Activate(routing_id(), session_id, request_id));
}

void RendererMediaSessionManager::Deactivate(
    int session_id,
    scoped_ptr<blink::WebMediaSessionDeactivateCallback> callback) {
  int request_id = pending_deactivation_requests_.Add(callback.release());
  Send(
      new MediaSessionHostMsg_Deactivate(routing_id(), session_id, request_id));
}

void RendererMediaSessionManager::OnDidActivate(int request_id, bool success) {
  DCHECK(pending_activation_requests_.Lookup(request_id)) << request_id;
  blink::WebMediaSessionActivateCallback* callback =
      pending_activation_requests_.Lookup(request_id);
  if (success) {
    callback->onSuccess();
  } else {
    callback->onError(
        blink::WebMediaSessionError(blink::WebMediaSessionError::Activate));
  }
  pending_activation_requests_.Remove(request_id);
}

void RendererMediaSessionManager::OnDidDeactivate(int request_id) {
  DCHECK(pending_deactivation_requests_.Lookup(request_id)) << request_id;
  pending_deactivation_requests_.Lookup(request_id)->onSuccess();
  pending_deactivation_requests_.Remove(request_id);
}

}  // namespace content
