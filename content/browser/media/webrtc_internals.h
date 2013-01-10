// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_H_
#define CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/process.h"
#include "base/values.h"
#include "content/common/content_export.h"

struct PeerConnectionInfo;

namespace content {

class WebRTCInternalsUIObserver;

// This is a singleton class running in the browser process.
// It collects peer connection infomation from the renderers,
// forwards the data to WebRTCInternalsUIObserver and
// sends data collecting commands to the renderers.
class CONTENT_EXPORT WebRTCInternals {
 public:
  static WebRTCInternals* GetInstance();

  // Methods called when peer connection status changes.
  void AddPeerConnection(base::ProcessId pid, const PeerConnectionInfo& info);
  void RemovePeerConnection(base::ProcessId pid, int lid);

  // Methods for adding or removing WebRTCInternalsUIObserver.
  void AddObserver(WebRTCInternalsUIObserver *observer);
  void RemoveObserver(WebRTCInternalsUIObserver *observer);

 private:
  friend struct DefaultSingletonTraits<WebRTCInternals>;

  WebRTCInternals();
  virtual ~WebRTCInternals();

  // Send updates to observers on UI thread.
  void SendUpdate(const std::string& command, base::Value* value);

  // Only the IO thread should access these fields.
  ObserverList<WebRTCInternalsUIObserver> observers_;
  base::ListValue peer_connection_data_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_H_
