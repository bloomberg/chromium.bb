// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_
#define CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_

#include <map>

#include "content/public/renderer/render_process_observer.h"
#include "third_party/libjingle/source/talk/app/webrtc/jsep.h"

namespace WebKit {
class WebFrame;
}  // namespace WebKit

namespace content {
class RTCMediaConstraints;
class RTCPeerConnectionHandler;

// This class collects data about each peer connection,
// sends it to the browser process, and handles messages
// from the browser process.
class PeerConnectionTracker : public RenderProcessObserver{
 public:
  PeerConnectionTracker();
  virtual ~PeerConnectionTracker();

  // Sends an update to the browser process when a PeerConnection has been
  // created in Javascript. The |pc_handler| is the handler object associated
  // with the PeerConnection, the |servers| are the server configurations used
  // to establish the connection, the |constraints| are the media constraints
  // used to initialize the PeerConnection, the |frame| is the WebFrame object
  // representing the page in which the PeerConnection is created.
  void RegisterPeerConnection(
      RTCPeerConnectionHandler* pc_handler,
      const std::vector<webrtc::JsepInterface::IceServer>& servers,
      const RTCMediaConstraints& constraints,
      const WebKit::WebFrame* frame);

  // Sends an update to the browser process when a PeerConnection has been
  // destroyed. The |pc_handler| is the handler object associated with the
  // PeerConnection.
  void UnregisterPeerConnection(RTCPeerConnectionHandler* pc_handler);

 private:
  // Assign a local ID to a peer connection so that the browser process can
  // uniquely identify a peer connection in the renderer process.
  int GetNextLocalID();

  // This map stores the local ID assigned to each RTCPeerConnectionHandler.
  std::map<RTCPeerConnectionHandler*, int> peer_connection_id_map_;

  // This keeps track of the next available local ID.
  int next_lid_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_