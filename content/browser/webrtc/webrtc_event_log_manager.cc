// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_event_log_manager.h"

#include <inttypes.h>

#include <iostream>
#include <limits>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

namespace {
inline void MaybeReplyWithBool(const base::Location& from_here,
                               base::OnceCallback<void(bool)> reply,
                               bool value) {
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, from_here,
                            base::BindOnce(std::move(reply), value));
  }
}

struct MaybeReplyWithBoolOnExit final {
  MaybeReplyWithBoolOnExit(base::OnceCallback<void(bool)> reply,
                           bool default_value)
      : reply(std::move(reply)), value(default_value) {}
  ~MaybeReplyWithBoolOnExit() {
    if (reply) {
      MaybeReplyWithBool(FROM_HERE, std::move(reply), value);
    }
  }
  base::OnceCallback<void(bool)> reply;
  bool value;
};
}  // namespace

base::LazyInstance<WebRtcEventLogManager>::Leaky g_webrtc_event_log_manager =
    LAZY_INSTANCE_INITIALIZER;

WebRtcEventLogManager* WebRtcEventLogManager::GetInstance() {
  return g_webrtc_event_log_manager.Pointer();
}

WebRtcEventLogManager::WebRtcEventLogManager()
    : clock_for_testing_(nullptr),
      local_logs_observer_(nullptr),
      max_local_log_file_size_bytes_(kDefaultMaxLocalLogFileSizeBytes),
      file_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebRtcEventLogManager::~WebRtcEventLogManager() {
  // This should never actually run, except in unit tests.
}

void WebRtcEventLogManager::PeerConnectionAdded(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::PeerConnectionAddedInternal,
                     base::Unretained(this), render_process_id, lid,
                     std::move(reply)));
}

void WebRtcEventLogManager::PeerConnectionRemoved(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::PeerConnectionRemovedInternal,
                     base::Unretained(this), render_process_id, lid,
                     std::move(reply)));
}

void WebRtcEventLogManager::EnableLocalLogging(
    base::FilePath base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base_path.empty());
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::EnableLocalLoggingInternal,
                     base::Unretained(this), base_path, max_file_size_bytes,
                     std::move(reply)));
}

void WebRtcEventLogManager::DisableLocalLogging(
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::DisableLocalLoggingInternal,
                     base::Unretained(this), std::move(reply)));
}

void WebRtcEventLogManager::OnWebRtcEventLogWrite(
    int render_process_id,
    int lid,
    const std::string& output,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcEventLogManager::WriteToLocalLogFile,
                                base::Unretained(this), render_process_id, lid,
                                output, std::move(reply)));
}

void WebRtcEventLogManager::SetLocalLogsObserver(LocalLogsObserver* observer,
                                                 base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::SetLocalLogsObserverInternal,
                     base::Unretained(this), observer, std::move(reply)));
}

void WebRtcEventLogManager::PeerConnectionAddedInternal(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  const auto result = active_peer_connections_.emplace(render_process_id, lid);

  if (!result.second) {  // PeerConnection already registered.
    MaybeReplyWithBool(FROM_HERE, std::move(reply), false);
    return;
  }

  if (!local_logs_base_path_.empty() &&
      local_logs_.size() < kMaxNumberLocalWebRtcEventLogFiles) {
    // Note that success/failure of starting the local log file is unrelated
    // to the success/failure of PeerConnectionAdded().
    StartLocalLogFile(render_process_id, lid);
  }

  MaybeReplyWithBool(FROM_HERE, std::move(reply), true);
}

void WebRtcEventLogManager::PeerConnectionRemovedInternal(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  const PeerConnectionKey key = PeerConnectionKey(render_process_id, lid);
  auto peer_connection = active_peer_connections_.find(key);

  if (peer_connection == active_peer_connections_.end()) {
    DCHECK(local_logs_.find(key) == local_logs_.end());
    MaybeReplyWithBool(FROM_HERE, std::move(reply), false);
    return;
  }

  auto local_log = local_logs_.find(key);
  if (local_log != local_logs_.end()) {
    // Note that success/failure of stopping the local log file is unrelated
    // to the success/failure of PeerConnectionRemoved().
    StopLocalLogFile(render_process_id, lid);
  }

  active_peer_connections_.erase(peer_connection);

  MaybeReplyWithBool(FROM_HERE, std::move(reply), true);
}

void WebRtcEventLogManager::EnableLocalLoggingInternal(
    base::FilePath base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(bool)> reply) {
  if (!local_logs_base_path_.empty()) {
    MaybeReplyWithBool(FROM_HERE, std::move(reply), false);
    return;
  }

  DCHECK_EQ(local_logs_.size(), 0u);

  local_logs_base_path_ = base_path;
  max_local_log_file_size_bytes_ = max_file_size_bytes;

  for (const PeerConnectionKey& peer_connection : active_peer_connections_) {
    if (local_logs_.size() >= kMaxNumberLocalWebRtcEventLogFiles) {
      break;
    }
    StartLocalLogFile(peer_connection.render_process_id, peer_connection.lid);
  }

  MaybeReplyWithBool(FROM_HERE, std::move(reply), true);
}

void WebRtcEventLogManager::DisableLocalLoggingInternal(
    base::OnceCallback<void(bool)> reply) {
  if (local_logs_base_path_.empty()) {
    MaybeReplyWithBool(FROM_HERE, std::move(reply), false);
    return;
  }

  // Perform an orderly closing of all active local logs.
  for (auto local_log = local_logs_.begin(); local_log != local_logs_.end();) {
    local_log = CloseLocalLogFile(local_log);
  }

  local_logs_base_path_.clear();  // Marks local-logging as disabled.
  max_local_log_file_size_bytes_ = kDefaultMaxLocalLogFileSizeBytes;

  MaybeReplyWithBool(FROM_HERE, std::move(reply), true);
}

void WebRtcEventLogManager::SetLocalLogsObserverInternal(
    LocalLogsObserver* observer,
    base::OnceClosure reply) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  local_logs_observer_ = observer;

  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(reply));
  }
}

void WebRtcEventLogManager::StartLocalLogFile(int render_process_id, int lid) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  // Add some information to the name given by the caller.
  base::FilePath file_path =
      GetLocalFilePath(local_logs_base_path_, render_process_id, lid);
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
  DCHECK(local_logs_.find({render_process_id, lid}) == local_logs_.end());
  local_logs_.emplace(key,
                      LogFile(std::move(file), max_local_log_file_size_bytes_));

  // The observer needs to be able to run on any TaskQueue.
  if (local_logs_observer_) {
    local_logs_observer_->OnLocalLogsStarted(key, file_path);
  }

  MaybeUpdateWebRtcEventLoggingState(render_process_id, lid);
}

void WebRtcEventLogManager::StopLocalLogFile(int render_process_id, int lid) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  auto it = local_logs_.find(PeerConnectionKey(render_process_id, lid));
  if (it == local_logs_.end()) {
    return;
  }
  CloseLocalLogFile(it);
  MaybeUpdateWebRtcEventLoggingState(render_process_id, lid);
}

void WebRtcEventLogManager::WriteToLocalLogFile(
    int render_process_id,
    int lid,
    const std::string& output,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_LE(output.length(),
            static_cast<size_t>(std::numeric_limits<int>::max()));

  // This object's destructor will ensure a reply (if |reply| is non-empty),
  // regardless of which path is taken out of the function.
  MaybeReplyWithBoolOnExit on_exit(std::move(reply), false);

  auto it = local_logs_.find(PeerConnectionKey(render_process_id, lid));
  if (it == local_logs_.end()) {
    return;
  }

  // Observe the file size limit, if any. Note that base::File's interface does
  // not allow writing more than numeric_limits<int>::max() bytes at a time.
  int output_len = static_cast<int>(output.length());  // DCHECKed above.
  LogFile& log_file = it->second;
  if (log_file.max_file_size_bytes != kUnlimitedFileSize) {
    DCHECK_LT(log_file.file_size_bytes, log_file.max_file_size_bytes);
    if (log_file.file_size_bytes + output.length() < log_file.file_size_bytes ||
        log_file.file_size_bytes + output.length() >
            log_file.max_file_size_bytes) {
      output_len = log_file.max_file_size_bytes - log_file.file_size_bytes;
    }
  }

  int written = log_file.file.WriteAtCurrentPos(output.c_str(), output_len);
  if (written < 0 || written != output_len) {  // Error
    LOG(WARNING) << "WebRTC event log output couldn't be written to local "
                    "file in its entirety.";
    CloseLocalLogFile(it);
    return;
  }

  // Truncated output due to exceeding the maximum is reported as an error - the
  // caller is interested to know that not all of its output was written,
  // regardless of the reason.
  on_exit.value = (static_cast<size_t>(written) == output.length());

  log_file.file_size_bytes += static_cast<size_t>(written);
  if (log_file.max_file_size_bytes != kUnlimitedFileSize) {
    DCHECK_LE(log_file.file_size_bytes, log_file.max_file_size_bytes);
    if (log_file.file_size_bytes >= log_file.max_file_size_bytes) {
      CloseLocalLogFile(it);
    }
  }
}

WebRtcEventLogManager::LocalLogFilesMap::iterator
WebRtcEventLogManager::CloseLocalLogFile(LocalLogFilesMap::iterator it) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  const PeerConnectionKey peer_connection = it->first;

  it->second.file.Flush();
  it = local_logs_.erase(it);  // file.Close() called by destructor.

  if (local_logs_observer_) {
    local_logs_observer_->OnLocalLogsStopped(peer_connection);
  }

  return it;
}

void WebRtcEventLogManager::MaybeUpdateWebRtcEventLoggingState(
    int render_process_id,
    int lid) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  // Currently we only support local logging, so the state always needs to be
  // updated when this function is called. When remote logging will be added,
  // we'll need to enable WebRTC logging only when the first of local/remote
  // logging is enabled, and disabled when both are disabled.
  const PeerConnectionKey key(render_process_id, lid);
  const bool enable = (local_logs_.find(key) != local_logs_.end());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::UpdateWebRtcEventLoggingState,
                     base::Unretained(this), render_process_id, lid, enable));
}

void WebRtcEventLogManager::UpdateWebRtcEventLoggingState(int render_process_id,
                                                          int lid,
                                                          bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host) {
    return;  // The host has been asynchronously removed; not a problem.
  }
  if (enabled) {
    host->Send(new PeerConnectionTracker_StartEventLogOutput(lid));
  } else {
    host->Send(new PeerConnectionTracker_StopEventLog(lid));
  }
}

base::FilePath WebRtcEventLogManager::GetLocalFilePath(
    const base::FilePath& base_path,
    int render_process_id,
    int lid) {
  // Expected to be called from within |file_task_runner_|, but there's no
  // real constraint for that.
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

void WebRtcEventLogManager::InjectClockForTesting(base::Clock* clock) {
  // Testing only; no need for threading guarantees (called before anything
  // could be put on the TQ).
  clock_for_testing_ = clock;
}

}  // namespace content
