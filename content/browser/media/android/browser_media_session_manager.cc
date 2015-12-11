// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_media_session_manager.h"

#include "content/common/media/media_session_messages_android.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

BrowserMediaSessionManager::BrowserMediaSessionManager(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

void BrowserMediaSessionManager::OnActivate(int session_id, int request_id) {
  NOTIMPLEMENTED();
  Send(new MediaSessionMsg_DidActivate(GetRoutingID(), request_id, false));
}

void BrowserMediaSessionManager::OnDeactivate(int session_id, int request_id) {
  NOTIMPLEMENTED();
  Send(new MediaSessionMsg_DidDeactivate(GetRoutingID(), request_id));
}

int BrowserMediaSessionManager::GetRoutingID() const {
  return render_frame_host_->GetRoutingID();
}

bool BrowserMediaSessionManager::Send(IPC::Message* msg) {
  return render_frame_host_->Send(msg);
}

}  // namespace content
