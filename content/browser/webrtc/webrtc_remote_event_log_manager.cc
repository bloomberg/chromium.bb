// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_remote_event_log_manager.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace content {

// TODO(eladalon): Block remote-bound logging on mobile devices.
// https://crbug.com/775415

namespace {
const base::FilePath::CharType kRemoteBoundLogSubDirectory[] =
    FILE_PATH_LITERAL("webrtc_event_logs");
}  // namespace

const size_t kMaxActiveRemoteBoundWebRtcEventLogs = 3;
const size_t kMaxPendingRemoteBoundWebRtcEventLogs = 5;
static_assert(kMaxActiveRemoteBoundWebRtcEventLogs <=
                  kMaxPendingRemoteBoundWebRtcEventLogs,
              "This assumption affects unit test coverage.");

const base::TimeDelta kRemoteBoundWebRtcEventLogsMaxRetention =
    base::TimeDelta::FromDays(3);

const base::FilePath::CharType kRemoteBoundLogExtension[] =
    FILE_PATH_LITERAL("log");

WebRtcRemoteEventLogManager::WebRtcRemoteEventLogManager(
    WebRtcRemoteEventLogsObserver* observer)
    : observer_(observer),
      uploader_factory_(new WebRtcEventLogUploaderImpl::Factory) {}

WebRtcRemoteEventLogManager::~WebRtcRemoteEventLogManager() {
  // TODO(eladalon): Purge from disk files which were being uploaded  while
  // destruction took place, thereby avoiding endless attempts to upload
  // the same file. https://crbug.com/775415
}

void WebRtcRemoteEventLogManager::EnableForBrowserContext(
    BrowserContextId browser_context,
    const base::FilePath& browser_context_dir) {
  const base::FilePath remote_bound_logs_dir =
      GetLogsDirectoryPath(browser_context_dir);
  if (!MaybeCreateLogsDirectory(remote_bound_logs_dir)) {
    LOG(WARNING)
        << "WebRtcRemoteEventLogManager couldn't create logs directory.";
    return;
  }
  AddPendingLogs(browser_context, remote_bound_logs_dir);
}

void WebRtcRemoteEventLogManager::DisableForBrowserContext(
    BrowserContextId browser_context) {
  auto active_it = active_logs_counts_.find(browser_context);
  if (active_it != active_logs_counts_.end()) {
    active_logs_counts_.erase(active_it);
  }
  auto pending_it = pending_logs_counts_.find(browser_context);
  if (pending_it != pending_logs_counts_.end()) {
    pending_logs_counts_.erase(pending_it);
  }
}

bool WebRtcRemoteEventLogManager::PeerConnectionAdded(int render_process_id,
                                                      int lid) {
  PrunePendingLogs();  // Infrequent event - good opportunity to prune.
  const auto result = active_peer_connections_.emplace(render_process_id, lid);
  return result.second;
}

bool WebRtcRemoteEventLogManager::PeerConnectionRemoved(
    int render_process_id,
    int lid,
    BrowserContextId browser_context) {
  PrunePendingLogs();  // Infrequent event - good opportunity to prune.

  const PeerConnectionKey key = PeerConnectionKey(render_process_id, lid);
  auto peer_connection = active_peer_connections_.find(key);

  if (peer_connection == active_peer_connections_.end()) {
    return false;
  }

  MaybeStopRemoteLogging(render_process_id, lid, browser_context);

  active_peer_connections_.erase(peer_connection);

  MaybeStartUploading();

  return true;
}

bool WebRtcRemoteEventLogManager::StartRemoteLogging(
    int render_process_id,
    int lid,
    BrowserContextId browser_context,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes) {
  // TODO(eladalon): Set a tighter limit (following discussion with rschriebman
  // and manj). https://crbug.com/775415
  if (max_file_size_bytes == kWebRtcEventLogManagerUnlimitedFileSize) {
    return false;
  }

  const PeerConnectionKey key(render_process_id, lid);

  // May not start logging inactive peer connections.
  if (active_peer_connections_.find(key) == active_peer_connections_.end()) {
    return false;
  }

  // May not restart active remote logs.
  auto it = active_logs_.find(key);
  if (it != active_logs_.end()) {
    LOG(ERROR) << "Remote logging already underway for (" << render_process_id
               << ", " << lid << ").";
    return false;
  }

  // This is a good opportunity to prune the list of pending logs, potentially
  // making room for this file.
  PrunePendingLogs();

  // Limit over concurrently active logs (across BrowserContext-s).
  if (active_logs_.size() >= kMaxActiveRemoteBoundWebRtcEventLogs) {
    return false;
  }

  // Limit over the number of pending logs (per BrowserContext). We count active
  // logs too, since they become pending logs once completed.
  if (active_logs_counts_[browser_context] +
          pending_logs_counts_[browser_context] >=
      kMaxPendingRemoteBoundWebRtcEventLogs) {
    return false;
  }

  return StartWritingLog(render_process_id, lid, browser_context,
                         browser_context_dir, max_file_size_bytes);
}

bool WebRtcRemoteEventLogManager::EventLogWrite(int render_process_id,
                                                int lid,
                                                const std::string& message) {
  auto it = active_logs_.find(PeerConnectionKey(render_process_id, lid));
  if (it == active_logs_.end()) {
    return false;
  }
  return WriteToLogFile(it, message);
}

void WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadComplete(
    const base::FilePath& file_path,
    bool upload_successful) {
  // Post a task to deallocate the uploader (can't do this directly,
  // because this function is a callback from the uploader), potentially
  // starting a new upload for the next file.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadCompleteInternal,
          base::Unretained(this)));

  // TODO(eladalon): Send indication of success/failure back to JS.
  // https://crbug.com/775415
}

void WebRtcRemoteEventLogManager::SetWebRtcEventLogUploaderFactoryForTesting(
    std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory) {
  uploader_factory_ = std::move(uploader_factory);

  // Unit tests would initially set a null uploader factory, so that files would
  // be kept around. Some tests would later change to a different factory
  // (e.g. one that always simulates upload failure); inthat case, we should
  // get rid of the null uploader, since it never terminates.
  uploader_.reset();
  MaybeStartUploading();
}

base::FilePath WebRtcRemoteEventLogManager::GetLogsDirectoryPath(
    const base::FilePath& browser_context_dir) {
  return browser_context_dir.Append(kRemoteBoundLogSubDirectory);
}

WebRtcRemoteEventLogManager::LogFilesMap::iterator
WebRtcRemoteEventLogManager::CloseLogFile(LogFilesMap::iterator it) {
  const PeerConnectionKey peer_connection = it->first;

  it->second.file.Flush();
  it = active_logs_.erase(it);  // file.Close() called by destructor.

  if (observer_) {
    observer_->OnRemoteLogStopped(peer_connection);
  }

  MaybeStartUploading();

  return it;
}

bool WebRtcRemoteEventLogManager::MaybeCreateLogsDirectory(
    const base::FilePath& remote_bound_logs_dir) {
  if (base::PathExists(remote_bound_logs_dir)) {
    if (!base::DirectoryExists(remote_bound_logs_dir)) {
      LOG(ERROR) << "Path for remote-bound logs is taken by a non-directory.";
      return false;
    }
  } else if (!base::CreateDirectory(remote_bound_logs_dir)) {
    LOG(ERROR) << "Failed to create the local directory for remote-bound logs.";
    return false;
  }

  // TODO(eladalon): Test for appropriate permissions. https://crbug.com/775415

  return true;
}

void WebRtcRemoteEventLogManager::AddPendingLogs(
    BrowserContextId browser_context,
    const base::FilePath& remote_bound_logs_dir) {
  DCHECK(active_logs_counts_.find(browser_context) ==
         active_logs_counts_.end());
  DCHECK(pending_logs_counts_.find(browser_context) ==
         pending_logs_counts_.end());

  active_logs_counts_.emplace(browser_context, 0);
  pending_logs_counts_.emplace(browser_context, 0);

  base::FilePath::StringType pattern =
      base::FilePath::StringType(FILE_PATH_LITERAL("*")) +
      kRemoteBoundLogExtension;
  base::FileEnumerator enumerator(remote_bound_logs_dir,
                                  /*recursive=*/false,
                                  base::FileEnumerator::FILES, pattern);

  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    const auto last_modified = enumerator.GetInfo().GetLastModifiedTime();
    pending_logs_.emplace(browser_context, path, last_modified);
    pending_logs_counts_[browser_context] += 1;
  }

  MaybeStartUploading();
}

bool WebRtcRemoteEventLogManager::StartWritingLog(
    int render_process_id,
    int lid,
    BrowserContextId browser_context,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes) {
  // Randomize a new filename. In the highly unlikely event that this filename
  // is already taken, it will be treated the same way as any other failure
  // to start the log file.
  // TODO(eladalon): Add a unit test for above comment. https://crbug.com/775415
  const std::string unique_filename =
      "event_log_" + std::to_string(base::RandUint64());
  const base::FilePath base_path = GetLogsDirectoryPath(browser_context_dir);
  const base::FilePath file_path = base_path.AppendASCII(unique_filename)
                                       .AddExtension(kRemoteBoundLogExtension);

  // Attempt to create the file.
  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_EXCLUSIVE_WRITE;
  base::File file(file_path, file_flags);
  if (!file.IsValid() || !file.created()) {
    LOG(WARNING) << "Couldn't create and/or open remote WebRTC event log file.";
    return false;
  }

  // Record that we're now writing this remote-bound log to this file.
  const PeerConnectionKey key(render_process_id, lid);
  const auto it = active_logs_.emplace(
      key, LogFile(file_path, std::move(file), max_file_size_bytes));
  DCHECK(it.second);
  active_logs_counts_[browser_context] += 1;

  observer_->OnRemoteLogStarted(PeerConnectionKey(render_process_id, lid),
                                file_path);

  return true;
}

void WebRtcRemoteEventLogManager::MaybeStopRemoteLogging(
    int render_process_id,
    int lid,
    BrowserContextId browser_context) {
  const PeerConnectionKey key(render_process_id, lid);
  const auto it = active_logs_.find(key);

  if (it == active_logs_.end()) {
    return;
  }

  it->second.file.Flush();
  it->second.file.Close();

  // The current time is a good enough approximation of the file's last
  // modification time.
  base::Time last_modified = base::Time::Now();

  pending_logs_.emplace(browser_context, it->second.path, last_modified);
  pending_logs_counts_[browser_context] += 1;

  active_logs_.erase(it);
  active_logs_counts_[browser_context] -= 1;

  observer_->OnRemoteLogStopped(PeerConnectionKey(render_process_id, lid));

  MaybeStartUploading();
}

void WebRtcRemoteEventLogManager::PrunePendingLogs() {
  const base::Time oldest_non_expired_timestamp =
      base::Time::Now() - kRemoteBoundWebRtcEventLogsMaxRetention;
  for (auto it = pending_logs_.begin(); it != pending_logs_.end();) {
    if (it->last_modified < oldest_non_expired_timestamp) {
      if (!base::DeleteFile(it->path, /*recursive=*/false)) {
        LOG(ERROR) << "Failed to delete " << it->path << ".";
      }
      pending_logs_counts_[it->browser_context] -= 1;
      it = pending_logs_.erase(it);
    } else {
      ++it;
    }
  }
}

bool WebRtcRemoteEventLogManager::UploadingAllowed() const {
  return active_peer_connections_.empty();
}

void WebRtcRemoteEventLogManager::MaybeStartUploading() {
  PrunePendingLogs();  // Avoid uploading freshly expired files.

  if (uploader_) {
    return;  // Upload already underway.
  }

  if (pending_logs_.empty()) {
    return;  // Nothing to upload.
  }

  if (!UploadingAllowed()) {
    return;
  }

  // The uploader takes ownership of the file; it's no longer considered to be
  // pending. (If the upload fails, the log will be deleted.)
  // TODO(eladalon): Add more refined retry behavior, so that we would not
  // delete the log permanently if the network is just down, on the one hand,
  // but also would not be uploading unlimited data on endless retries on the
  // other hand. https://crbug.com/775415
  uploader_ = uploader_factory_->Create(pending_logs_.begin()->path, this);
  pending_logs_.erase(pending_logs_.begin());
}

void WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadCompleteInternal() {
  uploader_.reset();
  MaybeStartUploading();
}

}  // namespace content
