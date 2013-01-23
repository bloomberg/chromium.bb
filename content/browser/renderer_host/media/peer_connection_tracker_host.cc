// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"

#include "base/process_util.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/webrtc_internals.h"

namespace content {

PeerConnectionTrackerHost::PeerConnectionTrackerHost() {
}

bool PeerConnectionTrackerHost::OnMessageReceived(const IPC::Message& message,
                                                  bool* message_was_ok) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_EX(PeerConnectionTrackerHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_AddPeerConnection,
                        OnAddPeerConnection)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_RemovePeerConnection,
                        OnRemovePeerConnection)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void PeerConnectionTrackerHost::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == PeerConnectionTrackerMsgStart)
    *thread = BrowserThread::UI;
}

PeerConnectionTrackerHost::~PeerConnectionTrackerHost() {
}

void PeerConnectionTrackerHost::OnAddPeerConnection(
    const PeerConnectionInfo& info) {
  if (!GetContentClient()->browser()->GetWebRTCInternals())
    return;

  GetContentClient()->browser()->GetWebRTCInternals()->
      AddPeerConnection(base::GetProcId(peer_handle()),
                        info.lid,
                        info.url,
                        info.servers,
                        info.constraints);
}

void PeerConnectionTrackerHost::OnRemovePeerConnection(int lid) {
  if (!GetContentClient()->browser()->GetWebRTCInternals())
    return;

  GetContentClient()->browser()->GetWebRTCInternals()->
      RemovePeerConnection(base::GetProcId(peer_handle()), lid);
}

}  // namespace content
