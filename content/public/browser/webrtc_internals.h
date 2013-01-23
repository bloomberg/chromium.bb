// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBRTC_INTERNALS_H_
#define CONTENT_PUBLIC_BROWSER_WEBRTC_INTERNALS_H_

#include "base/process.h"

namespace content {
// This interface is used to receive WebRTC updates.
// An embedder may implement it and return it from ContentBrowserClient.
class WebRTCInternals {
 public:
  // This method is called when a PeerConnection is created.
  // |pid| is the renderer process id, |lid| is the renderer local id used to
  // identify a PeerConnection, |url| is the url of the tab owning the
  // PeerConnection, |servers| is the servers configuration, |constraints| is
  // the media constraints used to initialize the PeerConnection.
  virtual void AddPeerConnection(base::ProcessId pid,
                                 int lid,
                                 const std::string& url,
                                 const std::string& servers,
                                 const std::string& constraints) = 0;

  // This method is called when PeerConnection is destroyed.
  // |pid| is the renderer process id, |lid| is the renderer local id.
  virtual void RemovePeerConnection(base::ProcessId pid, int lid) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBRTC_INTERNALSH_
