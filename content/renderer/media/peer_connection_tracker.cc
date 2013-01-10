// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/peer_connection_tracker.h"

#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

using webrtc::MediaConstraintsInterface;

namespace content {

PeerConnectionTracker::PeerConnectionTracker() : next_lid_(1) {
}

PeerConnectionTracker::~PeerConnectionTracker() {
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const std::vector<webrtc::JsepInterface::IceServer>& servers,
    const RTCMediaConstraints& constraints,
    const WebKit::WebFrame* frame) {
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  PeerConnectionInfo info;

  info.lid = GetNextLocalID();
  for (size_t i = 0; i < servers.size(); ++i) {
    info.servers += servers[i].uri + ";";
  }
  MediaConstraintsInterface::Constraints mandatory = constraints.GetMandatory();
  if (!mandatory.empty()) {
    info.constraints = "mandatory: ";
    for (size_t i = 0; i < mandatory.size(); ++i) {
      info.constraints += "[" + mandatory[i].key + ":"
                              + mandatory[i].value + "]";
    }
  }
  MediaConstraintsInterface::Constraints optional = constraints.GetOptional();
  if (!optional.empty()) {
    info.constraints += "optional: ";
    for (size_t i = 0; i < optional.size(); ++i) {
      info.constraints += "[" + optional[i].key + ":"
                              + optional[i].value + "]";
    }
  }
  info.url = frame->document().url().spec();
  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_AddPeerConnection(info));

  DCHECK(peer_connection_id_map_.find(pc_handler) ==
         peer_connection_id_map_.end());
  peer_connection_id_map_[pc_handler] = info.lid;
}

void PeerConnectionTracker::UnregisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler) {
  DVLOG(1) << "PeerConnectionTracker::UnregisterPeerConnection()";

  std::map<RTCPeerConnectionHandler*, int>::iterator it =
      peer_connection_id_map_.find(pc_handler);

  if (it == peer_connection_id_map_.end()) {
    // The PeerConnection might not have been registered if its initilization
    // failed.
    return;
  }

  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_RemovePeerConnection(it->second));

  peer_connection_id_map_.erase(it);
}

int PeerConnectionTracker::GetNextLocalID() {
  return next_lid_++;
}

}  // namespace content
