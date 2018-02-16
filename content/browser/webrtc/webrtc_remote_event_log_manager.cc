// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_remote_event_log_manager.h"

#include <algorithm>
#include <limits>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// TODO(eladalon): Block remote-bound logging on mobile devices.
// https://crbug.com/775415

namespace {
const base::FilePath::CharType kRemoteBoundLogSubDirectory[] =
    FILE_PATH_LITERAL("webrtc_event_logs");

// Purge from local disk a log file which could not be properly started
// (e.g. error encountered when attempting to write the log header).
void DiscardLogFile(base::File* file, const base::FilePath& file_path) {
  file->Close();
  if (!base::DeleteFile(file_path, /*recursive=*/false)) {
    LOG(ERROR) << "Failed to delete " << file_path << ".";
  }
}

bool ValidLogParameters(size_t max_file_size_bytes,
                        const std::string& metadata) {
  // TODO(eladalon): Set a tighter limit (following discussion with rschriebman
  // and manj). https://crbug.com/775415
  if (max_file_size_bytes == kWebRtcEventLogManagerUnlimitedFileSize) {
    return false;
  }

  if (metadata.length() > 0xFFFFFFu) {
    // We use three bytes to encode the length of the metadata.
    LOG(ERROR) << "Metadata must be less than 2^24 bytes.";
    return false;
  }

  if (metadata.size() + kRemoteBoundLogFileHeaderSizeBytes >=
      max_file_size_bytes) {
    LOG(ERROR) << "Max file size and metadata must leave room for event log.";
    return false;
  }

  return true;
}
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

const uint8_t kRemoteBoundWebRtcEventLogFileVersion = 0;

const size_t kRemoteBoundLogFileHeaderSizeBytes = sizeof(uint32_t);

WebRtcRemoteEventLogManager::WebRtcRemoteEventLogManager(
    WebRtcRemoteEventLogsObserver* observer)
    : observer_(observer),
      uploader_factory_(new WebRtcEventLogUploaderImpl::Factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(io_task_sequence_checker_);
}

WebRtcRemoteEventLogManager::~WebRtcRemoteEventLogManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(eladalon): Purge from disk files which were being uploaded  while
  // destruction took place, thereby avoiding endless attempts to upload
  // the same file. https://crbug.com/775415
}

void WebRtcRemoteEventLogManager::EnableForBrowserContext(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  DCHECK(!BrowserContextEnabled(browser_context_id)) << "Already enabled.";

  const base::FilePath remote_bound_logs_dir =
      GetLogsDirectoryPath(browser_context_dir);
  if (!MaybeCreateLogsDirectory(remote_bound_logs_dir)) {
    LOG(WARNING)
        << "WebRtcRemoteEventLogManager couldn't create logs directory.";
    return;
  }

  AddPendingLogs(browser_context_id, remote_bound_logs_dir);

  enabled_browser_contexts_.insert(browser_context_id);
}

// TODO(eladalon): Add unit tests. https://crbug.com/775415
void WebRtcRemoteEventLogManager::DisableForBrowserContext(
    BrowserContextId browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  if (!BrowserContextEnabled(browser_context_id)) {
    return;  // Enabling may have failed due to lacking permissions.
  }

  enabled_browser_contexts_.erase(browser_context_id);

#if DCHECK_IS_ON()
  // All of the RPHs associated with this BrowserContext must already have
  // exited, which should have implicitly stopped all active logs.
  auto pred = [browser_context_id](decltype(active_logs_)::value_type& log) {
    return log.first.browser_context_id == browser_context_id;
  };
  DCHECK(std::count_if(active_logs_.begin(), active_logs_.end(), pred) == 0u);
#endif

  // Pending logs for this BrowserContext are no longer eligible for upload.
  // (Active uploads, if any, are not affected.)
  for (auto it = pending_logs_.begin(); it != pending_logs_.end();) {
    if (it->browser_context_id == browser_context_id) {
      it = pending_logs_.erase(it);
    } else {
      ++it;
    }
  }
}

bool WebRtcRemoteEventLogManager::PeerConnectionAdded(
    const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  PrunePendingLogs();  // Infrequent event - good opportunity to prune.
  const auto result = active_peer_connections_.insert(key);
  return result.second;
}

bool WebRtcRemoteEventLogManager::PeerConnectionRemoved(
    const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  PrunePendingLogs();  // Infrequent event - good opportunity to prune.

  const auto peer_connection = active_peer_connections_.find(key);
  if (peer_connection == active_peer_connections_.end()) {
    return false;
  }

  MaybeStopRemoteLogging(key);

  active_peer_connections_.erase(peer_connection);

  MaybeStartUploading();

  return true;
}

bool WebRtcRemoteEventLogManager::StartRemoteLogging(
    const PeerConnectionKey& key,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    const std::string& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  if (!ValidLogParameters(max_file_size_bytes, metadata)) {
    return false;
  }

  if (!BrowserContextEnabled(key.browser_context_id)) {
    return false;
  }

  // May not start logging inactive peer connections.
  if (active_peer_connections_.find(key) == active_peer_connections_.end()) {
    return false;
  }

  // May not restart active remote logs.
  auto it = active_logs_.find(key);
  if (it != active_logs_.end()) {
    LOG(ERROR) << "Remote logging already underway for ("
               << key.render_process_id << ", " << key.lid << ").";
    return false;
  }

  // This is a good opportunity to prune the list of pending logs, potentially
  // making room for this file.
  PrunePendingLogs();

  if (!AdditionalActiveLogAllowed(key.browser_context_id)) {
    return false;
  }

  return StartWritingLog(key, browser_context_dir, max_file_size_bytes,
                         metadata);
}

bool WebRtcRemoteEventLogManager::EventLogWrite(const PeerConnectionKey& key,
                                                const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  auto it = active_logs_.find(key);
  if (it == active_logs_.end()) {
    return false;
  }
  return WriteToLogFile(it, message);
}

void WebRtcRemoteEventLogManager::RenderProcessHostExitedDestroyed(
    int render_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  // Closing files will call MaybeStartUploading(). Avoid letting that upload
  // any recently expired files.
  PrunePendingLogs();

  // Remove all of the peer connections associated with this render process.
  // It's important to do this before closing the actual files, because closing
  // files can trigger a new upload if no active peer connections are present.
  auto pc_it = active_peer_connections_.begin();
  while (pc_it != active_peer_connections_.end()) {
    if (pc_it->render_process_id == render_process_id) {
      pc_it = active_peer_connections_.erase(pc_it);
    } else {
      ++pc_it;
    }
  }

  // Close all of the files that were associated with peer connections which
  // belonged to this render process.
  auto log_it = active_logs_.begin();
  while (log_it != active_logs_.end()) {
    if (log_it->first.render_process_id == render_process_id) {
      log_it = CloseLogFile(log_it);
    } else {
      ++log_it;
    }
  }

  // Though CloseLogFile() calls this, it's important to also do this
  // explicitly, since it could be that no files were closed, but some
  // active PeerConnections that were suppressing uploading are now gone.
  MaybeStartUploading();
}

void WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadComplete(
    const base::FilePath& file_path,
    bool upload_successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  uploader_factory_ = std::move(uploader_factory);

  // Unit tests would initially set a null uploader factory, so that files would
  // be kept around. Some tests would later change to a different factory
  // (e.g. one that always simulates upload failure); in that case, we should
  // get rid of the null uploader, since it never terminates.
  uploader_.reset();
  MaybeStartUploading();
}

base::FilePath WebRtcRemoteEventLogManager::GetLogsDirectoryPath(
    const base::FilePath& browser_context_dir) {
  return browser_context_dir.Append(kRemoteBoundLogSubDirectory);
}

bool WebRtcRemoteEventLogManager::BrowserContextEnabled(
    BrowserContextId browser_context_id) const {
  const auto it = enabled_browser_contexts_.find(browser_context_id);
  return it != enabled_browser_contexts_.cend();
}

WebRtcRemoteEventLogManager::LogFilesMap::iterator
WebRtcRemoteEventLogManager::CloseLogFile(LogFilesMap::iterator it) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

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
    BrowserContextId browser_context_id,
    const base::FilePath& remote_bound_logs_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  base::FilePath::StringType pattern =
      base::FilePath::StringType(FILE_PATH_LITERAL("*")) +
      kRemoteBoundLogExtension;
  base::FileEnumerator enumerator(remote_bound_logs_dir,
                                  /*recursive=*/false,
                                  base::FileEnumerator::FILES, pattern);

  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    const auto last_modified = enumerator.GetInfo().GetLastModifiedTime();
    auto it = pending_logs_.emplace(browser_context_id, path, last_modified);
    DCHECK(it.second);  // No pre-existing entry.
  }

  MaybeStartUploading();
}

bool WebRtcRemoteEventLogManager::StartWritingLog(
    const PeerConnectionKey& key,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    const std::string& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  // WriteAtCurrentPos() only allows writing up to max-int at a time. We could
  // iterate to do more, but we don't expect to ever need to, so it's easier
  // to disallow it.
  if (metadata.length() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    LOG(WARNING) << "Metadata too long to be written in one go.";
    return false;
  }

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

  const uint32_t header_host_order =
      static_cast<uint32_t>(metadata.length()) |
      (kRemoteBoundWebRtcEventLogFileVersion << 24);
  static_assert(kRemoteBoundLogFileHeaderSizeBytes == sizeof(uint32_t),
                "Restructure this otherwise.");
  char header[sizeof(uint32_t)];
  base::WriteBigEndian<uint32_t>(header, header_host_order);
  int written = file.WriteAtCurrentPos(header, arraysize(header));
  if (written != arraysize(header)) {
    LOG(WARNING) << "Failed to write header to log file.";
    DiscardLogFile(&file, file_path);
    return false;
  }

  const int metadata_length = static_cast<int>(metadata.length());
  written = file.WriteAtCurrentPos(metadata.c_str(), metadata_length);
  if (written != metadata_length) {
    LOG(WARNING) << "Failed to write metadata to log file.";
    DiscardLogFile(&file, file_path);
    return false;
  }

  // Record that we're now writing this remote-bound log to this file.
  const size_t header_and_metadata_size_bytes =
      kRemoteBoundLogFileHeaderSizeBytes + metadata_length;
  const auto it = active_logs_.emplace(
      key, LogFile(file_path, std::move(file), max_file_size_bytes,
                   header_and_metadata_size_bytes));
  DCHECK(it.second);

  observer_->OnRemoteLogStarted(key, file_path);

  return true;
}

void WebRtcRemoteEventLogManager::MaybeStopRemoteLogging(
    const PeerConnectionKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  const auto it = active_logs_.find(key);
  if (it == active_logs_.end()) {
    return;
  }

  it->second.file.Flush();
  it->second.file.Close();

  // The current time is a good enough approximation of the file's last
  // modification time.
  const base::Time last_modified = base::Time::Now();

  // The stopped log becomes a pending log. It is no longer an active log.
  const auto emplace_result = pending_logs_.emplace(
      key.browser_context_id, it->second.path, last_modified);
  DCHECK(emplace_result.second);  // No pre-existing entry.
  active_logs_.erase(it);

  observer_->OnRemoteLogStopped(key);

  MaybeStartUploading();
}

void WebRtcRemoteEventLogManager::PrunePendingLogs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  const base::Time oldest_non_expired_timestamp =
      base::Time::Now() - kRemoteBoundWebRtcEventLogsMaxRetention;
  for (auto it = pending_logs_.begin(); it != pending_logs_.end();) {
    if (it->last_modified < oldest_non_expired_timestamp) {
      if (!base::DeleteFile(it->path, /*recursive=*/false)) {
        LOG(ERROR) << "Failed to delete " << it->path << ".";
      }
      it = pending_logs_.erase(it);
    } else {
      ++it;
    }
  }
}

bool WebRtcRemoteEventLogManager::AdditionalActiveLogAllowed(
    BrowserContextId browser_context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

  // Limit over concurrently active logs (across BrowserContext-s).
  if (active_logs_.size() >= kMaxActiveRemoteBoundWebRtcEventLogs) {
    return false;
  }

  // Limit over the number of pending logs (per BrowserContext). We count active
  // logs too, since they become pending logs once completed.
  const size_t active_count = std::count_if(
      active_logs_.begin(), active_logs_.end(),
      [browser_context_id](const decltype(active_logs_)::value_type& log) {
        return log.first.browser_context_id == browser_context_id;
      });
  const size_t pending_count = std::count_if(
      pending_logs_.begin(), pending_logs_.end(),
      [browser_context_id](const decltype(pending_logs_)::value_type& log) {
        return log.browser_context_id == browser_context_id;
      });
  return active_count + pending_count < kMaxPendingRemoteBoundWebRtcEventLogs;
}

bool WebRtcRemoteEventLogManager::UploadingAllowed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  return active_peer_connections_.empty();
}

void WebRtcRemoteEventLogManager::MaybeStartUploading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_task_sequence_checker_);
  uploader_.reset();
  MaybeStartUploading();
}

}  // namespace content
