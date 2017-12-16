// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_COMMON_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_COMMON_H_

#include <tuple>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT extern const size_t kWebRtcEventLogManagerUnlimitedFileSize;

CONTENT_EXPORT extern const size_t kDefaultMaxLocalLogFileSizeBytes;
CONTENT_EXPORT extern const size_t kMaxNumberLocalWebRtcEventLogFiles;

// For a given Chrome session, this is a unique key for PeerConnections.
// It's not, however, unique between sessions (after Chrome is restarted).
struct WebRtcEventLogPeerConnectionKey {
  constexpr WebRtcEventLogPeerConnectionKey(int render_process_id, int lid)
      : render_process_id(render_process_id), lid(lid) {}

  bool operator==(const WebRtcEventLogPeerConnectionKey& other) const {
    return std::tie(render_process_id, lid) ==
           std::tie(other.render_process_id, other.lid);
  }

  bool operator<(const WebRtcEventLogPeerConnectionKey& other) const {
    return std::tie(render_process_id, lid) <
           std::tie(other.render_process_id, other.lid);
  }

  int render_process_id;
  int lid;  // Renderer-local PeerConnection ID.
};

// An observer for notifications of local log files being started/stopped, and
// the paths which will be used for these logs.
class WebRtcLocalEventLogsObserver {
 public:
  virtual ~WebRtcLocalEventLogsObserver() = default;
  virtual void OnLocalLogStarted(WebRtcEventLogPeerConnectionKey pc_key,
                                 base::FilePath file_path) = 0;
  virtual void OnLocalLogStopped(WebRtcEventLogPeerConnectionKey pc_key) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_COMMON_H_
