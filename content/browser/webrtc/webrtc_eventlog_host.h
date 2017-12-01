// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_EVENTLOG_HOST_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_EVENTLOG_HOST_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

// This class is used to enable and disable WebRTC event logs on each of the
// PeerConnections in the render process. This class and all its members should
// only be accessed from the Browser UI thread.
class CONTENT_EXPORT WebRTCEventLogHost {
 public:
  explicit WebRTCEventLogHost(int render_process_id);
  ~WebRTCEventLogHost();

  // Starts WebRTC event logging to local files for all current and future
  // PeerConnections on the render process. A base file_path must be supplied,
  // which will be extended to include several identifiers to ensure uniqueness.
  // If local WebRTC event logging was already in progress, this call will
  // return false and have no other effect.
  bool StartLocalWebRtcEventLogging(const base::FilePath& base_path);

  // Stops recording of WebRTC event logs into local files for each
  // PeerConnection on the render process. If local WebRTC event logging wasn't
  // active, this call will return false.
  bool StopLocalWebRtcEventLogging();

  // This function should be used to notify the WebRTCEventLogHost object that a
  // PeerConnection was created in the corresponding render process.
  void PeerConnectionAdded(int peer_connection_local_id);

  // This function should be used to notify the WebRTCEventLogHost object that a
  // PeerConnection was removed in the corresponding render process.
  void PeerConnectionRemoved(int peer_connection_local_id);

  base::WeakPtr<WebRTCEventLogHost> GetWeakPtr();

 private:
  // Tracks which peer connections have log files associated.
  struct PeerConnectionKey {
    bool operator==(const PeerConnectionKey& other) const {
      return render_process_id == other.render_process_id &&
             peer_connection_local_id == other.peer_connection_local_id;
    }
    int render_process_id;
    int peer_connection_local_id;
  };
  static std::vector<PeerConnectionKey>& ActivePeerConnectionsWithLogFiles();

  // Actually start the eventlog for a single PeerConnection using the path
  // stored in base_file_path_.
  void StartEventLogForPeerConnection(int peer_connection_local_id);

  // Management of |active_peer_connections_with_log_files_|, which tracks
  // which PCs have associated log files.
  // WebRtcEventLogRemoved() returns whether actual removal took place.
  void WebRtcEventLogAdded(int peer_connection_local_id);
  bool WebRtcEventLogRemoved(int peer_connection_local_id);

  // The render process ID that this object is associated with.
  const int render_process_id_;

  // In case new PeerConnections are created during logging, the path is needed
  // to enable logging for them.
  base::FilePath base_file_path_;

  // The local identifiers of all the currently active PeerConnections.
  std::vector<int> active_peer_connection_local_ids_;

  // Track if the RTC event log is currently active.
  bool rtc_event_logging_enabled_;

  base::WeakPtrFactory<WebRTCEventLogHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCEventLogHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_EVENTLOG_HOST_H_
