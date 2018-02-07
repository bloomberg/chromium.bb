// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_COMMON_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_COMMON_H_

#include <map>
#include <string>
#include <tuple>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

// This file is intended for:
// 1. Code shared between WebRtcEventLogManager, WebRtcLocalEventLogManager
//    and WebRtcRemoteEventLogManager.
// 2. Code specific to either of the above classes, but which also needs
//    to be seen by unit tests (such as constants).

namespace content {

CONTENT_EXPORT extern const size_t kWebRtcEventLogManagerUnlimitedFileSize;

CONTENT_EXPORT extern const size_t kDefaultMaxLocalLogFileSizeBytes;
CONTENT_EXPORT extern const size_t kMaxNumberLocalWebRtcEventLogFiles;

// Limit over the number of concurrently active (currently being written to
// disk) remote-bound log files. This limits IO operations, and so it is
// applied globally (all browser contexts are limited together).
CONTENT_EXPORT extern const size_t kMaxActiveRemoteBoundWebRtcEventLogs;

// Limit over the number of pending logs (logs stored on disk and awaiting to
// be uploaded to a remote server). This limit avoids excessive storage. If a
// user chooses to have multiple profiles (and hence browser contexts) on a
// system, it is assumed that the user has enough storage to accommodate
// the increased storage consumption that comes with it. Therefore, this
// limit is applied per browser context.
CONTENT_EXPORT extern const size_t kMaxPendingRemoteBoundWebRtcEventLogs;

// The file extension to be associated with remote-bound logs while they are
// kept on local disk.
CONTENT_EXPORT extern const base::FilePath::CharType kRemoteBoundLogExtension[];

// Remote-bound event logs will not be uploaded if the time since their last
// modification (meaning the time when they were completed) exceeds this value.
// Such expired files will be purged from disk when examined.
CONTENT_EXPORT extern const base::TimeDelta
    kRemoteBoundWebRtcEventLogsMaxRetention;

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
  virtual void OnLocalLogStarted(WebRtcEventLogPeerConnectionKey key,
                                 const base::FilePath& file_path) = 0;
  virtual void OnLocalLogStopped(WebRtcEventLogPeerConnectionKey key) = 0;

 protected:
  virtual ~WebRtcLocalEventLogsObserver() = default;
};

// An observer for notifications of remote-bound log files being
// started/stopped. The start event would likely only interest unit tests
// (because it exposes the randomized filename to them). The stop event is of
// general interest, because it would often mean that WebRTC can stop sending
// us event logs for this peer connection.
// Some cases where OnRemoteLogStopped would be called include:
// 1. The PeerConnection has become inactive.
// 2. The file's maximum size has been reached.
// 3. Any type of error while writing to the file.
class WebRtcRemoteEventLogsObserver {
 public:
  virtual void OnRemoteLogStarted(WebRtcEventLogPeerConnectionKey key,
                                  const base::FilePath& file_path) = 0;
  virtual void OnRemoteLogStopped(WebRtcEventLogPeerConnectionKey key) = 0;

 protected:
  virtual ~WebRtcRemoteEventLogsObserver() = default;
};

struct LogFile {
  LogFile(const base::FilePath& path,
          base::File file,
          size_t max_file_size_bytes)
      : path(path),
        file(std::move(file)),
        max_file_size_bytes(max_file_size_bytes),
        file_size_bytes(0) {}
  const base::FilePath path;
  base::File file;
  const size_t max_file_size_bytes;
  size_t file_size_bytes;
};

// WebRtcLocalEventLogManager and WebRtcRemoteEventLogManager share some logic
// when it comes to handling of files on disk.
class LogFileWriter {
 protected:
  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;
  using LogFilesMap = std::map<PeerConnectionKey, LogFile>;

  virtual ~LogFileWriter() = default;

  // Given a peer connection and its associated log file, and given a log
  // fragment that should be written to the log file, attempt to write to
  // the log file (return value indicates success/failure).
  // If an error occurs, or if the file reaches its capacity, CloseLogFile()
  // will be called, closing the file.
  bool WriteToLogFile(LogFilesMap::iterator it, const std::string& message);

  // Called when WriteToLogFile() either encounters an error, or if the file's
  // intended capacity is reached. It indicates to the inheriting class that
  // the file should also be purged from its set of active log files.
  // The function should return an iterator to the next element in the set
  // of active logs. This makes the function more useful, allowing it to be
  // used when iterating and closing several log files.
  virtual LogFilesMap::iterator CloseLogFile(LogFilesMap::iterator it) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_COMMON_H_
