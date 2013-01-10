// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_

#include "content/public/browser/browser_message_filter.h"

struct PeerConnectionInfo;

namespace content {
class WebRTCInternals;

// This class is the host for PeerConnectionTracker in the browser process
// managed by RenderProcessHostImpl. It passes IPC messages between
// WebRTCInternals and PeerConnectionTracker.
class PeerConnectionTrackerHost : public BrowserMessageFilter {
 public:
  PeerConnectionTrackerHost();

  // content::BrowserMessageFilter override.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 protected:
  virtual ~PeerConnectionTrackerHost();

 private:
  // Handlers for peer connection messages coming from the renderer.
  void OnAddPeerConnection(const PeerConnectionInfo& info);
  void OnRemovePeerConnection(int lid);

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionTrackerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_PEER_CONNECTION_TRACKER_HOST_H_
