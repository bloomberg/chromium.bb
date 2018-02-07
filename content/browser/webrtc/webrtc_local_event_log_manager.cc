// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_local_event_log_manager.h"

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

#if defined(OS_ANDROID)
const size_t kDefaultMaxLocalLogFileSizeBytes = 10000000;
const size_t kMaxNumberLocalWebRtcEventLogFiles = 3;
#else
const size_t kDefaultMaxLocalLogFileSizeBytes = 60000000;
const size_t kMaxNumberLocalWebRtcEventLogFiles = 5;
#endif

WebRtcLocalEventLogManager::WebRtcLocalEventLogManager(
    WebRtcLocalEventLogsObserver* observer)
    : observer_(observer),
      clock_for_testing_(nullptr),
      max_log_file_size_bytes_(kDefaultMaxLocalLogFileSizeBytes) {}

WebRtcLocalEventLogManager::~WebRtcLocalEventLogManager() {
  // This should never actually run, except in unit tests.
}

bool WebRtcLocalEventLogManager::PeerConnectionAdded(int render_process_id,
                                                     int lid) {
  const auto result = active_peer_connections_.emplace(render_process_id, lid);

  if (!result.second) {  // PeerConnection already registered.
    return false;
  }

  if (!base_path_.empty() &&
      log_files_.size() < kMaxNumberLocalWebRtcEventLogFiles) {
    // Note that success/failure of starting the local log file is unrelated
    // to the success/failure of PeerConnectionAdded().
    StartLogFile(render_process_id, lid);
  }

  return true;
}

bool WebRtcLocalEventLogManager::PeerConnectionRemoved(int render_process_id,
                                                       int lid) {
  const PeerConnectionKey key = PeerConnectionKey(render_process_id, lid);
  auto peer_connection = active_peer_connections_.find(key);

  if (peer_connection == active_peer_connections_.end()) {
    DCHECK(log_files_.find(key) == log_files_.end());
    return false;
  }

  auto local_log = log_files_.find(key);
  if (local_log != log_files_.end()) {
    // Note that success/failure of stopping the local log file is unrelated
    // to the success/failure of PeerConnectionRemoved().
    CloseLogFile(local_log);
  }

  active_peer_connections_.erase(peer_connection);

  return true;
}

bool WebRtcLocalEventLogManager::EnableLogging(const base::FilePath& base_path,
                                               size_t max_file_size_bytes) {
  if (!base_path_.empty()) {
    return false;
  }

  DCHECK_EQ(log_files_.size(), 0u);

  base_path_ = base_path;
  max_log_file_size_bytes_ = max_file_size_bytes;

  for (const PeerConnectionKey& peer_connection : active_peer_connections_) {
    if (log_files_.size() >= kMaxNumberLocalWebRtcEventLogFiles) {
      break;
    }
    StartLogFile(peer_connection.render_process_id, peer_connection.lid);
  }

  return true;
}

bool WebRtcLocalEventLogManager::DisableLogging() {
  if (base_path_.empty()) {
    return false;
  }

  for (auto local_log = log_files_.begin(); local_log != log_files_.end();) {
    local_log = CloseLogFile(local_log);
  }

  base_path_.clear();  // Marks local-logging as disabled.
  max_log_file_size_bytes_ = kDefaultMaxLocalLogFileSizeBytes;

  return true;
}

bool WebRtcLocalEventLogManager::EventLogWrite(int render_process_id,
                                               int lid,
                                               const std::string& message) {
  auto it = log_files_.find(PeerConnectionKey(render_process_id, lid));
  if (it == log_files_.end()) {
    return false;
  }
  return WriteToLogFile(it, message);
}

void WebRtcLocalEventLogManager::SetClockForTesting(base::Clock* clock) {
  clock_for_testing_ = clock;
}

void WebRtcLocalEventLogManager::StartLogFile(int render_process_id, int lid) {
  // Add some information to the name given by the caller.
  base::FilePath file_path = GetFilePath(base_path_, render_process_id, lid);
  CHECK(!file_path.empty()) << "Couldn't set path for local WebRTC log file.";

  // In the unlikely case that this filename is already taken, find a unique
  // number to append to the filename, if possible.
  int unique_number =
      base::GetUniquePathNumber(file_path, base::FilePath::StringType());
  if (unique_number < 0) {
    return;  // No available file path was found.
  } else if (unique_number != 0) {
    // The filename is taken, but a unique number was found.
    // TODO(eladalon): Fix the way the unique number is used.
    // https://crbug.com/785333
    file_path = file_path.InsertBeforeExtension(FILE_PATH_LITERAL(" (") +
                                                IntToStringType(unique_number) +
                                                FILE_PATH_LITERAL(")"));
  }

  // Attempt to create the file.
  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_EXCLUSIVE_WRITE;
  base::File file(file_path, file_flags);
  if (!file.IsValid() || !file.created()) {
    LOG(WARNING) << "Couldn't create and/or open local WebRTC event log file.";
    return;
  }

  const PeerConnectionKey key(render_process_id, lid);

  // If the file was successfully created, it's now ready to be written to.
  DCHECK(log_files_.find({render_process_id, lid}) == log_files_.end());
  log_files_.emplace(
      key, LogFile(file_path, std::move(file), max_log_file_size_bytes_));

  // The observer needs to be able to run on any TaskQueue.
  if (observer_) {
    observer_->OnLocalLogStarted(key, file_path);
  }
}

WebRtcLocalEventLogManager::LogFilesMap::iterator
WebRtcLocalEventLogManager::CloseLogFile(LogFilesMap::iterator it) {
  const PeerConnectionKey peer_connection = it->first;

  it->second.file.Flush();
  it = log_files_.erase(it);  // file.Close() called by destructor.

  if (observer_) {
    observer_->OnLocalLogStopped(peer_connection);
  }

  return it;
}

base::FilePath WebRtcLocalEventLogManager::GetFilePath(
    const base::FilePath& base_path,
    int render_process_id,
    int lid) {
  base::Time::Exploded now;
  if (clock_for_testing_) {
    clock_for_testing_->Now().LocalExplode(&now);
  } else {
    base::Time::Now().LocalExplode(&now);
  }

  // [user_defined]_[date]_[time]_[pid]_[lid].log
  char stamp[100];
  int written =
      base::snprintf(stamp, arraysize(stamp), "%04d%02d%02d_%02d%02d_%d_%d",
                     now.year, now.month, now.day_of_month, now.hour,
                     now.minute, render_process_id, lid);
  CHECK_GT(written, 0);
  CHECK_LT(static_cast<size_t>(written), arraysize(stamp));

  return base_path.InsertBeforeExtension(FILE_PATH_LITERAL("_"))
      .InsertBeforeExtensionASCII(base::StringPiece(stamp))
      .AddExtension(FILE_PATH_LITERAL("log"));
}

}  // namespace content
