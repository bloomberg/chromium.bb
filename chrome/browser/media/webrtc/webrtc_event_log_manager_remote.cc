// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_remote.h"

#include <algorithm>
#include <iterator>
#include <limits>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

// TODO(crbug.com/775415): Change max back to (1u << 29) after resolving the
// issue where we read the entire file into memory.
const size_t kMaxRemoteLogFileSizeBytes = 50000000u;

namespace {
const base::TimeDelta kDefaultProactivePruningDelta =
    base::TimeDelta::FromMinutes(5);

const base::TimeDelta kDefaultWebRtcRemoteEventLogUploadDelay =
    base::TimeDelta::FromSeconds(30);

bool AreLogParametersValid(size_t max_file_size_bytes,
                           std::string* error_message) {
  if (max_file_size_bytes == kWebRtcEventLogManagerUnlimitedFileSize) {
    LOG(WARNING) << "Unlimited file sizes not allowed for remote-bound logs.";
    *error_message = kStartRemoteLoggingFailureUnlimitedSizeDisallowed;
    return false;
  }

  if (max_file_size_bytes > kMaxRemoteLogFileSizeBytes) {
    LOG(WARNING) << "File size exceeds maximum allowed.";
    *error_message = kStartRemoteLoggingFailureMaxSizeTooLarge;
    return false;
  }

  return true;
}

base::TimeDelta GetProactivePruningDelta() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kWebRtcRemoteEventLogProactivePruningDelta)) {
    const std::string delta_seconds_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ::switches::kWebRtcRemoteEventLogProactivePruningDelta);
    int64_t seconds;
    if (base::StringToInt64(delta_seconds_str, &seconds) && seconds >= 0) {
      return base::TimeDelta::FromSeconds(seconds);
    } else {
      LOG(WARNING) << "Proactive pruning delta could not be parsed.";
    }
  }

  return kDefaultProactivePruningDelta;
}

base::TimeDelta GetUploadDelay() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kWebRtcRemoteEventLogUploadDelayMs)) {
    const std::string delta_seconds_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ::switches::kWebRtcRemoteEventLogUploadDelayMs);
    int64_t ms;
    if (base::StringToInt64(delta_seconds_str, &ms) && ms >= 0) {
      return base::TimeDelta::FromMilliseconds(ms);
    } else {
      LOG(WARNING) << "Upload delay could not be parsed; using default delay.";
    }
  }

  return kDefaultWebRtcRemoteEventLogUploadDelay;
}

bool TimePointInRange(const base::Time& time_point,
                      const base::Time& range_begin,
                      const base::Time& range_end) {
  DCHECK(!time_point.is_null());
  DCHECK(range_begin.is_null() || range_end.is_null() ||
         range_begin <= range_end);
  return (range_begin.is_null() || range_begin <= time_point) &&
         (range_end.is_null() || time_point < range_end);
}

// Create a random identifier of 32 hexadecimal (uppercase) characters.
std::string CreateLogId() {
  // UnguessableToken's interface makes no promisses over case. We therefore
  // convert, even if the current implementation does not require it.
  std::string log_id =
      base::ToUpperASCII(base::UnguessableToken::Create().ToString());
  DCHECK_EQ(log_id.size(), 32u);
  DCHECK_EQ(log_id.find_first_not_of("0123456789ABCDEF"), std::string::npos);
  return log_id;
}

// Do not attempt to upload when there is no active connection.
// Do not attempt to upload if the connection is known to be a mobile one.
// Err on the side of caution with unknown connection types (by not uploading).
// Note #1: A device may have multiple connections, so this is not bullet-proof.
// Note #2: Does not attempt to recognize mobile hotspots.
bool UploadSupportedUsingConnectionType(
    network::mojom::ConnectionType connection) {
  if (connection == network::mojom::ConnectionType::CONNECTION_ETHERNET ||
      connection == network::mojom::ConnectionType::CONNECTION_WIFI) {
    return true;
  }
  return false;
}
}  // namespace

const size_t kMaxActiveRemoteBoundWebRtcEventLogs = 3;
const size_t kMaxPendingRemoteBoundWebRtcEventLogs = 5;
static_assert(kMaxActiveRemoteBoundWebRtcEventLogs <=
                  kMaxPendingRemoteBoundWebRtcEventLogs,
              "This assumption affects unit test coverage.");

const base::TimeDelta kRemoteBoundWebRtcEventLogsMaxRetention =
    base::TimeDelta::FromDays(7);

const base::FilePath::CharType kRemoteBoundWebRtcEventLogFileNamePrefix[] =
    FILE_PATH_LITERAL("webrtc_event_log_");
const base::FilePath::CharType kRemoteBoundWebRtcEventLogExtension[] =
    FILE_PATH_LITERAL("log");

WebRtcRemoteEventLogManager::WebRtcRemoteEventLogManager(
    WebRtcRemoteEventLogsObserver* observer,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : upload_suppression_disabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              ::switches::kWebRtcRemoteEventLogUploadNoSuppression)),
      proactive_prune_scheduling_delta_(GetProactivePruningDelta()),
      upload_delay_(GetUploadDelay()),
      proactive_prune_scheduling_started_(false),
      observer_(observer),
      network_connection_tracker_(nullptr),
      uploading_supported_for_connection_type_(false),
      scheduled_upload_tasks_(0),
      task_runner_(task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Proactive pruning would not do anything at the moment; it will be started
  // with the first enabled browser context. This will all have the benefit
  // of doing so on |task_runner_| rather than the UI thread.
}

WebRtcRemoteEventLogManager::~WebRtcRemoteEventLogManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/775415): Purge from disk files which were being uploaded
  // while destruction took place, thereby avoiding endless attempts to upload
  // the same file.

  if (network_connection_tracker_) {
    // * |network_connection_tracker_| might already have posted a task back
    //   to us, but it will not run, because |task_runner_| has already been
    //   stopped.
    // * RemoveNetworkConnectionObserver() should generally be called on the
    //   same thread as AddNetworkConnectionObserver(), but in this case it's
    //   okay to remove on a separate thread, because this only happens during
    //   Chrome shutdown, when no others tasks are running; there can be no
    //   concurrently executing notification from the tracker.
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  }
}

void WebRtcRemoteEventLogManager::SetNetworkConnectionTracker(
    content::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(network_connection_tracker);
  DCHECK(!network_connection_tracker_);

  // |this| is only destroyed (on the UI thread) after |task_runner_| stops,
  // so both base::Unretained(this) and AddNetworkConnectionObserver() are safe.

  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);

  auto callback =
      base::BindOnce(&WebRtcRemoteEventLogManager::OnConnectionChanged,
                     base::Unretained(this));
  network::mojom::ConnectionType connection_type;
  const bool sync_answer = network_connection_tracker_->GetConnectionType(
      &connection_type, std::move(callback));

  if (sync_answer) {
    OnConnectionChanged(connection_type);
  }

  // Because this happens while enabling the first browser context, there is no
  // necessity to consider uploading yet.
  DCHECK_EQ(enabled_browser_contexts_.size(), 0u);
}

void WebRtcRemoteEventLogManager::SetUrlRequestContextGetter(
    net::URLRequestContextGetter* context_getter) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_getter);
  DCHECK(!uploader_factory_);
  uploader_factory_ =
      std::make_unique<WebRtcEventLogUploaderImpl::Factory>(context_getter);
}

void WebRtcRemoteEventLogManager::EnableForBrowserContext(
    BrowserContextId browser_context_id,
    const base::FilePath& browser_context_dir) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(uploader_factory_) << "SetUrlRequestContextGetter() not called.";
  DCHECK(!BrowserContextEnabled(browser_context_id)) << "Already enabled.";

  const base::FilePath remote_bound_logs_dir =
      GetRemoteBoundWebRtcEventLogsDir(browser_context_dir);
  if (!MaybeCreateLogsDirectory(remote_bound_logs_dir)) {
    LOG(WARNING)
        << "WebRtcRemoteEventLogManager couldn't create logs directory.";
    return;
  }

  AddPendingLogs(browser_context_id, remote_bound_logs_dir);

  enabled_browser_contexts_.insert(browser_context_id);

  if (!proactive_prune_scheduling_delta_.is_zero() &&
      !proactive_prune_scheduling_started_) {
    proactive_prune_scheduling_started_ = true;
    RecurringPendingLogsPrune();
  }
}

// TODO(crbug.com/775415): Add unit tests.
void WebRtcRemoteEventLogManager::DisableForBrowserContext(
    BrowserContextId browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

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

  // Active logs may have been removed, which could remove upload suppression,
  // or pending logs which were about to be uploaded may have been removed,
  // so uploading may no longer be possible.
  ManageUploadSchedule();
}

bool WebRtcRemoteEventLogManager::PeerConnectionAdded(
    const PeerConnectionKey& key,
    const std::string& peer_connection_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  PrunePendingLogs();  // Infrequent event - good opportunity to prune.

  const auto result = active_peer_connections_.emplace(key, peer_connection_id);

  // An upload about to start might need to be suppressed.
  ManageUploadSchedule();

  return result.second;
}

bool WebRtcRemoteEventLogManager::PeerConnectionRemoved(
    const PeerConnectionKey& key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  PrunePendingLogs();  // Infrequent event - good opportunity to prune.

  const auto peer_connection = active_peer_connections_.find(key);
  if (peer_connection == active_peer_connections_.end()) {
    return false;
  }

  MaybeStopRemoteLogging(key);

  active_peer_connections_.erase(peer_connection);

  ManageUploadSchedule();  // Suppression might have been removed.

  return true;
}

bool WebRtcRemoteEventLogManager::StartRemoteLogging(
    int render_process_id,
    BrowserContextId browser_context_id,
    const std::string& peer_connection_id,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    std::string* log_id,
    std::string* error_message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(log_id);
  DCHECK(log_id->empty());
  DCHECK(error_message);
  DCHECK(error_message->empty());

  if (!AreLogParametersValid(max_file_size_bytes, error_message)) {
    // |error_message| will have been set by AreLogParametersValid().
    DCHECK(!error_message->empty()) << "AreLogParametersValid() reported an "
                                       "error without an error message.";
    return false;
  }

  if (!BrowserContextEnabled(browser_context_id)) {
    *error_message = kStartRemoteLoggingFailureGeneric;
    return false;
  }

  PeerConnectionKey key;
  if (!FindPeerConnection(render_process_id, peer_connection_id, &key)) {
    *error_message = kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
    return false;
  }

  // May not restart active remote logs.
  auto it = active_logs_.find(key);
  if (it != active_logs_.end()) {
    LOG(ERROR) << "Remote logging already underway for ("
               << key.render_process_id << ", " << key.lid << ").";
    *error_message = kStartRemoteLoggingFailureAlreadyLogging;
    return false;
  }

  // This is a good opportunity to prune the list of pending logs, potentially
  // making room for this file.
  PrunePendingLogs();

  if (!AdditionalActiveLogAllowed(key.browser_context_id)) {
    // Intentionally use a generic error, so as to not leak information such
    // as there being too many other peer connections on other tabs that might
    // also be logging.
    *error_message = kStartRemoteLoggingFailureGeneric;
    return false;
  }

  return StartWritingLog(key, browser_context_dir, max_file_size_bytes, log_id,
                         error_message);
}

bool WebRtcRemoteEventLogManager::EventLogWrite(const PeerConnectionKey& key,
                                                const std::string& message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto it = active_logs_.find(key);
  if (it == active_logs_.end()) {
    return false;
  }

  const bool write_successful = it->second.Write(message);

  if (!write_successful || it->second.MaxSizeReached()) {
    CloseLogFile(it, /*make_pending=*/true);
    ManageUploadSchedule();
  }

  return write_successful;
}

void WebRtcRemoteEventLogManager::ClearCacheForBrowserContext(
    BrowserContextId browser_context_id,
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  MaybeCancelActiveLogs(delete_begin, delete_end, browser_context_id);
  MaybeRemovePendingLogs(delete_begin, delete_end, browser_context_id);
  MaybeCancelUpload(delete_begin, delete_end, browser_context_id);
}

void WebRtcRemoteEventLogManager::RenderProcessHostExitedDestroyed(
    int render_process_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Remove all of the peer connections associated with this render process.
  // It's important to do this before closing the actual files, because closing
  // files can trigger a new upload if no active peer connections are present.
  auto pc_it = active_peer_connections_.begin();
  while (pc_it != active_peer_connections_.end()) {
    if (pc_it->first.render_process_id == render_process_id) {
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
      log_it = CloseLogFile(log_it, /*make_pending=*/true);
    } else {
      ++log_it;
    }
  }

  ManageUploadSchedule();
}

void WebRtcRemoteEventLogManager::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  // Even if switching from WiFi to Ethernet, or between to WiFi connections,
  // reset the timer (if running) until an upload is permissible due to stable
  // upload-supporting conditions.
  time_when_upload_conditions_met_ = base::TimeTicks();

  uploading_supported_for_connection_type_ =
      UploadSupportedUsingConnectionType(type);

  ManageUploadSchedule();

  // TODO(crbug.com/775415): Support pausing uploads when connection goes down,
  // or switches to an unsupported connection type.
}

void WebRtcRemoteEventLogManager::SetWebRtcEventLogUploaderFactoryForTesting(
    std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  uploader_factory_ = std::move(uploader_factory);
}

void WebRtcRemoteEventLogManager::UploadConditionsHoldForTesting(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(callback), UploadConditionsHold()));
}

bool WebRtcRemoteEventLogManager::BrowserContextEnabled(
    BrowserContextId browser_context_id) const {
  const auto it = enabled_browser_contexts_.find(browser_context_id);
  return it != enabled_browser_contexts_.cend();
}

WebRtcRemoteEventLogManager::LogFilesMap::iterator
WebRtcRemoteEventLogManager::CloseLogFile(LogFilesMap::iterator it,
                                          bool make_pending) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const PeerConnectionKey peer_connection = it->first;  // Copy, not reference.

  it->second.Close();

  if (make_pending) {
    // The current time is a good enough approximation of the file's last
    // modification time.
    const base::Time last_modified = base::Time::Now();

    // The stopped log becomes a pending log.
    const auto emplace_result = pending_logs_.emplace(
        peer_connection.browser_context_id, it->second.path(), last_modified);
    DCHECK(emplace_result.second);  // No pre-existing entry.
  }

  it = active_logs_.erase(it);

  if (observer_) {
    observer_->OnRemoteLogStopped(peer_connection);
  }

  return it;
}

bool WebRtcRemoteEventLogManager::MaybeCreateLogsDirectory(
    const base::FilePath& remote_bound_logs_dir) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (base::PathExists(remote_bound_logs_dir)) {
    if (!base::DirectoryExists(remote_bound_logs_dir)) {
      LOG(ERROR) << "Path for remote-bound logs is taken by a non-directory.";
      return false;
    }
  } else if (!base::CreateDirectory(remote_bound_logs_dir)) {
    LOG(ERROR) << "Failed to create the local directory for remote-bound logs.";
    return false;
  }

  // TODO(crbug.com/775415): Test for appropriate permissions.

  return true;
}

void WebRtcRemoteEventLogManager::AddPendingLogs(
    BrowserContextId browser_context_id,
    const base::FilePath& remote_bound_logs_dir) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::FilePath::StringType pattern =
      base::FilePath::StringType(FILE_PATH_LITERAL("*")) +
      base::FilePath::kExtensionSeparator + kRemoteBoundWebRtcEventLogExtension;
  base::FileEnumerator enumerator(remote_bound_logs_dir,
                                  /*recursive=*/false,
                                  base::FileEnumerator::FILES, pattern);

  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    const auto last_modified = enumerator.GetInfo().GetLastModifiedTime();
    auto it = pending_logs_.emplace(browser_context_id, path, last_modified);
    DCHECK(it.second);  // No pre-existing entry.
  }

  ManageUploadSchedule();
}

bool WebRtcRemoteEventLogManager::StartWritingLog(
    const PeerConnectionKey& key,
    const base::FilePath& browser_context_dir,
    size_t max_file_size_bytes,
    std::string* log_id,
    std::string* error_message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // The log is assigned a universally unique ID (with high probability).
  const std::string id = CreateLogId();

  // Use the log ID as part of the filename. In the highly unlikely event that
  // this filename is already taken, it will be treated the same way as any
  // other failure to start the log file.
  // TODO(crbug.com/775415): Add a unit test for above comment.
  const base::FilePath base_path =
      GetRemoteBoundWebRtcEventLogsDir(browser_context_dir);
  const base::FilePath file_path =
      base_path.Append(kRemoteBoundWebRtcEventLogFileNamePrefix)
          .InsertBeforeExtensionASCII(id)
          .AddExtension(kRemoteBoundWebRtcEventLogExtension);

  // Attempt to create the file.
  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_EXCLUSIVE_WRITE;
  base::File file(file_path, file_flags);
  if (!file.IsValid() || !file.created()) {
    LOG(WARNING) << "Couldn't create and/or open remote WebRTC event log file.";
    // Intentionally using a generic error; look for other places where it's
    // set for an explanation why.
    *error_message = kStartRemoteLoggingFailureGeneric;
    return false;
  }

  // The log is now ACTIVE.
  LogFile log_file(file_path, std::move(file), max_file_size_bytes);
  const auto it = active_logs_.emplace(key, std::move(log_file));
  DCHECK(it.second);

  observer_->OnRemoteLogStarted(key, it.first->second.path());

  *log_id = id;
  return true;
}

void WebRtcRemoteEventLogManager::MaybeStopRemoteLogging(
    const PeerConnectionKey& key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const auto it = active_logs_.find(key);
  if (it == active_logs_.end()) {
    return;
  }

  CloseLogFile(it, /*make_pending=*/true);

  ManageUploadSchedule();
}

void WebRtcRemoteEventLogManager::PrunePendingLogs() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  MaybeRemovePendingLogs(
      base::Time::Min(),
      base::Time::Now() - kRemoteBoundWebRtcEventLogsMaxRetention);
}

void WebRtcRemoteEventLogManager::RecurringPendingLogsPrune() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!proactive_prune_scheduling_delta_.is_zero());
  DCHECK(proactive_prune_scheduling_started_);

  PrunePendingLogs();

  // |this| is only destroyed (on the UI thread) after |task_runner_| stops,
  // so both base::Unretained(this) is safe.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebRtcRemoteEventLogManager::RecurringPendingLogsPrune,
                     base::Unretained(this)),
      proactive_prune_scheduling_delta_);
}

void WebRtcRemoteEventLogManager::MaybeRemovePendingLogs(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    base::Optional<BrowserContextId> browser_context_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  for (auto it = pending_logs_.begin(); it != pending_logs_.end();) {
    if (LogFileMatchesFilter(it->browser_context_id, it->last_modified,
                             browser_context_id, delete_begin, delete_end)) {
      DVLOG(1) << "Removing " << it->path << ".";
      if (!base::DeleteFile(it->path, /*recursive=*/false)) {
        LOG(ERROR) << "Failed to delete " << it->path << ".";
      }
      it = pending_logs_.erase(it);
    } else {
      DVLOG(1) << "Keeping " << it->path << " on disk.";
      ++it;
    }
  }

  // The last pending log might have been removed.
  if (!UploadConditionsHold()) {
    time_when_upload_conditions_met_ = base::TimeTicks();
  }
}

void WebRtcRemoteEventLogManager::MaybeCancelActiveLogs(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    BrowserContextId browser_context_id) {
  for (auto it = active_logs_.begin(); it != active_logs_.end();) {
    // Since the file is active, assume it's still being modified.
    if (LogFileMatchesFilter(it->first.browser_context_id, base::Time::Now(),
                             browser_context_id, delete_begin, delete_end)) {
      const base::FilePath log_file_path = it->second.path();
      it = CloseLogFile(it, /*make_pending=*/false);
      if (!base::DeleteFile(log_file_path, /*recursive=*/false)) {
        LOG(ERROR) << "Failed to delete " << log_file_path << ".";
      }
    } else {
      ++it;
    }
  }
}

void WebRtcRemoteEventLogManager::MaybeCancelUpload(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    base::Optional<BrowserContextId> browser_context_id) {
  if (uploader_) {
    const WebRtcLogFileInfo& info = uploader_->GetWebRtcLogFileInfo();
    if (LogFileMatchesFilter(info.browser_context_id, info.last_modified,
                             browser_context_id, delete_begin, delete_end)) {
      // Cancel the upload. (If the upload has asynchronously completed by now,
      // the uploader must have posted a task back to our queue to delete it
      // and move on to the next file; cancellation is reported as unsucessful
      // in that case.)
      const bool cancelled = uploader_->Cancel();
      if (cancelled) {
        uploader_.reset();
        ManageUploadSchedule();
      }
    }
  }
}

bool WebRtcRemoteEventLogManager::LogFileMatchesFilter(
    BrowserContextId log_browser_context_id,
    const base::Time& log_last_modification,
    base::Optional<BrowserContextId> filter_browser_context_id,
    const base::Time& filter_range_begin,
    const base::Time& filter_range_end) const {
  if (filter_browser_context_id &&
      *filter_browser_context_id != log_browser_context_id) {
    return false;
  }
  return TimePointInRange(log_last_modification, filter_range_begin,
                          filter_range_end);
}

bool WebRtcRemoteEventLogManager::AdditionalActiveLogAllowed(
    BrowserContextId browser_context_id) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

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

bool WebRtcRemoteEventLogManager::UploadSuppressed() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return !upload_suppression_disabled_ && !active_peer_connections_.empty();
}

bool WebRtcRemoteEventLogManager::UploadConditionsHold() const {
  return !uploader_ && !pending_logs_.empty() && !UploadSuppressed() &&
         uploading_supported_for_connection_type_;
}

void WebRtcRemoteEventLogManager::ManageUploadSchedule() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  PrunePendingLogs();  // Avoid uploading freshly expired files.

  if (!UploadConditionsHold()) {
    time_when_upload_conditions_met_ = base::TimeTicks();
    return;
  }

  if (!time_when_upload_conditions_met_.is_null()) {
    // Conditions have been holding for a while; MaybeStartUploading() has
    // already been scheduled when |time_when_upload_conditions_met_| was set.
    return;
  }

  ++scheduled_upload_tasks_;

  time_when_upload_conditions_met_ = base::TimeTicks::Now();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebRtcRemoteEventLogManager::MaybeStartUploading,
                     base::Unretained(this)),
      upload_delay_);
}

void WebRtcRemoteEventLogManager::MaybeStartUploading() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GT(scheduled_upload_tasks_, 0u);

  // Since MaybeStartUploading() was scheduled, conditions might have stopped
  // holding at some point. They may have even stopped and started several times
  // while the currently running task was scheduled, meaning several tasks could
  // be pending now, only the last of which should really end up uploading.

  if (time_when_upload_conditions_met_.is_null()) {
    // Conditions no longer hold; no way to know how many (now irrelevant) other
    // similar tasks are pending, if any.
  } else if (base::TimeTicks::Now() - time_when_upload_conditions_met_ <
             upload_delay_) {
    // Conditions have stopped holding, then started holding again; there has
    // to be a more recent task scheduled, that will take over later.
    DCHECK_GT(scheduled_upload_tasks_, 1u);
  } else {
    // It's up to the rest of the code to turn |scheduled_upload_tasks_| off
    // if the conditions have at some point stopped holding, or it wouldn't
    // know to turn it on when they resume.
    DCHECK(UploadConditionsHold());

    // When the upload we're about to start finishes, there will be another
    // delay of length |upload_delay_| before the next one starts.
    time_when_upload_conditions_met_ = base::TimeTicks();

    // |this| is only destroyed (on the UI thread) after |task_runner_| stops,
    // so base::Unretained(this) is safe. (|uploader_| and |uploader_factory_|
    // live on |task_runner_|.)
    auto callback = base::BindOnce(
        &WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadComplete,
        base::Unretained(this));

    // The uploader takes ownership of the file; it's no longer considered to be
    // pending. (If the upload fails, the log will be deleted.)
    // TODO(crbug.com/775415): Add more refined retry behavior, so that we would
    // not delete the log permanently if the network is just down, on the one
    // hand, but also would not be uploading unlimited data on endless retries
    // on the other hand.
    // TODO(crbug.com/775415): Rename the file before uploading, so that we
    // would not retry the upload after restarting Chrome, if the upload is
    // interrupted.
    uploader_ =
        uploader_factory_->Create(*pending_logs_.begin(), std::move(callback));
    pending_logs_.erase(pending_logs_.begin());
  }

  --scheduled_upload_tasks_;
}

void WebRtcRemoteEventLogManager::OnWebRtcEventLogUploadComplete(
    const base::FilePath& log_file,
    bool upload_successful) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  uploader_.reset();
  ManageUploadSchedule();
}

bool WebRtcRemoteEventLogManager::FindPeerConnection(
    int render_process_id,
    const std::string& peer_connection_id,
    PeerConnectionKey* key) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const auto it = FindNextPeerConnection(active_peer_connections_.cbegin(),
                                         render_process_id, peer_connection_id);
  if (it == active_peer_connections_.cend()) {
    return false;
  }

  // Make sure that the peer connection ID is unique for the renderer process
  // in which it exists, though not necessarily between renderer processes.
  // (The helper exists just to allow this DCHECK.)
  DCHECK(FindNextPeerConnection(std::next(it), render_process_id,
                                peer_connection_id) ==
         active_peer_connections_.cend());

  *key = it->first;
  return true;
}

std::map<WebRtcEventLogPeerConnectionKey, const std::string>::const_iterator
WebRtcRemoteEventLogManager::FindNextPeerConnection(
    std::map<PeerConnectionKey, const std::string>::const_iterator begin,
    int render_process_id,
    const std::string& peer_connection_id) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const auto end = active_peer_connections_.cend();
  for (auto it = begin; it != end; ++it) {
    if (it->first.render_process_id == render_process_id &&
        it->second == peer_connection_id) {
      return it;
    }
  }
  return end;
}
