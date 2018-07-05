// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_REMOTE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_REMOTE_H_

#include <map>
#include <set>
#include <vector>

#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_uploader.h"
#include "content/public/browser/network_connection_tracker.h"

// TODO(crbug.com/775415): Avoid uploading logs when Chrome shutdown imminent.

namespace net {
class URLRequestContextGetter;
}  // namespace net

class WebRtcRemoteEventLogManager final
    : public content::NetworkConnectionTracker::NetworkConnectionObserver {
  using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;
  using LogFilesMap = std::map<WebRtcEventLogPeerConnectionKey, LogFile>;
  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;

 public:
  WebRtcRemoteEventLogManager(
      WebRtcRemoteEventLogsObserver* observer,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WebRtcRemoteEventLogManager() override;

  // Sets a content::NetworkConnectionTracker which will be used to track
  // network connectivity.
  // Must not be called more than once.
  // Must be called before any call to EnableForBrowserContext().
  void SetNetworkConnectionTracker(
      content::NetworkConnectionTracker* network_connection_tracker);

  // Sets a net::URLRequestContextGetter which will be used for uploads.
  // Must not be called more than once.
  // Must be called before any call to EnableForBrowserContext().
  void SetUrlRequestContextGetter(net::URLRequestContextGetter* context_getter);

  // Enables remote-bound logging for a given BrowserContext. Logs stored during
  // previous sessions become eligible for upload, and recording of new logs for
  // peer connections associated with this BrowserContext, in the
  // BrowserContext's user-data directory, becomes possible.
  // This method would typically be called when a BrowserContext is initialized.
  void EnableForBrowserContext(BrowserContextId browser_context_id,
                               const base::FilePath& browser_context_dir);

  // Enables remote-bound logging for a given BrowserContext. Pending logs from
  // earlier (while it was enabled) may still be uploaded, but no additional
  // logs will be created.
  void DisableForBrowserContext(BrowserContextId browser_context_id);

  // Called to inform |this| of peer connections being added/removed.
  // This information is used to:
  // 1. Make decisions about when to upload previously finished logs.
  // 2. When a peer connection is removed, if it was being logged, its log
  //    changes from ACTIVE to PENDING.
  // The return value of both methods indicates only the consistency of the
  // information with previously received information (e.g. can't remove a
  // peer connection that was never added, etc.).
  bool PeerConnectionAdded(const PeerConnectionKey& key,
                           const std::string& peer_connection_id);
  bool PeerConnectionRemoved(const PeerConnectionKey& key);

  // Attempt to start logging the WebRTC events of an active peer connection.
  // Logging is subject to several restrictions:
  // 1. May not log more than kMaxNumberActiveRemoteWebRtcEventLogFiles logs
  //    at the same time.
  // 2. Each browser context may have only kMaxPendingLogFilesPerBrowserContext
  //    pending logs. Since active logs later become pending logs, it is also
  //    forbidden to start a remote-bound log that would, once completed, become
  //    a pending log that would exceed that limit.
  // 3. The maximum file size must be sensible.
  //
  // If all of the restrictions were observed, and if a file was successfully
  // created, true will be return.
  //
  // If the call succeeds, the log's identifier will be written to |log_id|.
  // The log identifier is exactly 32 uppercase ASCII characters from the
  // ranges 0-9 and A-F.
  //
  // If the call fails, an error message is written to |error_message|.
  // The error message will be specific to the failure (as opposed to a generic
  // one) is produced only if that error message is useful for the caller:
  // * Bad parameters.
  // * Function called at a time when the caller could know it would fail,
  //   such as for a peer connection that was already logged.
  // We intentionally avoid giving specific errors in some cases, so as
  // to avoid leaking information such as having too many active and/or
  // pending logs.
  bool StartRemoteLogging(int render_process_id,
                          BrowserContextId browser_context_id,
                          const std::string& peer_connection_id,
                          const base::FilePath& browser_context_dir,
                          size_t max_file_size_bytes,
                          std::string* log_id,
                          std::string* error_message);

  // If an active remote-bound log exists for the given peer connection, this
  // will append |message| to that log.
  // If writing |message| to the log would exceed the log's maximum allowed
  // size, the write is disallowed and the file is closed instead (and changes
  // from ACTIVE to PENDING).
  // If the log file's capacity is exhausted as a result of this function call,
  // or if a write error occurs, the file is closed, and the remote-bound log
  // changes from ACTIVE to PENDING.
  // True is returned if and only if |message| was written in its entirety to
  // an active log.
  bool EventLogWrite(const PeerConnectionKey& key, const std::string& message);

  // Clear PENDING WebRTC event logs associated with a given browser context,
  // in a given time range, then post |reply| back to the thread from which
  // the method was originally invoked (which can be any thread).
  // Log files currently being written are *not* interrupted.
  // Active uploads *are* interrupted.
  void ClearCacheForBrowserContext(BrowserContextId browser_context_id,
                                   const base::Time& delete_begin,
                                   const base::Time& delete_end);

  // An implicit PeerConnectionRemoved() on all of the peer connections that
  // were associated with the renderer process.
  void RenderProcessHostExitedDestroyed(int render_process_id);

  // content::NetworkConnectionTracker::NetworkConnectionObserver implementation
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Unit tests may use this to inject null uploaders, or ones which are
  // directly controlled by the unit test (succeed or fail according to the
  // test's needs).
  // Note that for simplicity's sake, this may be called from outside the
  // task queue on which this object lives (WebRtcEventLogManager::task_queue_).
  // Therefore, if a test calls this, it should call it before it initializes
  // any BrowserContext with pending log files in its directory.
  void SetWebRtcEventLogUploaderFactoryForTesting(
      std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory);

  // Exposes UploadConditionsHold() to unit tests. See WebRtcEventLogManager's
  // documentation for the rationale.
  void UploadConditionsHoldForTesting(base::OnceCallback<void(bool)> callback);

 private:
  // Checks whether a browser context has already been enabled via a call to
  // EnableForBrowserContext(), and not yet disabled using a call to
  // DisableForBrowserContext().
  bool BrowserContextEnabled(BrowserContextId browser_context_id) const;

  // Closes an active log file, changing its state from ACTIVE to PENDING.
  // Returns an iterator to the next ACTIVE file.
  LogFilesMap::iterator CloseLogFile(LogFilesMap::iterator it,
                                     bool make_pending);

  // Attempts to create the directory where we'll write the logs, if it does
  // not already exist. Returns true if the directory exists (either it already
  // existed, or it was successfully created).
  bool MaybeCreateLogsDirectory(const base::FilePath& remote_bound_logs_dir);

  // Scans the user data directory associated with this BrowserContext for
  // remote-bound logs that were created during previous Chrome sessions.
  // This function does *not* protect against manipulation by the user,
  // who might seed the directory with more files than were permissible.
  void AddPendingLogs(BrowserContextId browser_context_id,
                      const base::FilePath& remote_bound_logs_dir);

  // Attempts the creation of a locally stored file into which a remote-bound
  // log may be written. The log-identifier is returned if successful, the empty
  // string otherwise.
  bool StartWritingLog(const PeerConnectionKey& key,
                       const base::FilePath& browser_context_dir,
                       size_t max_file_size_bytes,
                       std::string* log_id,
                       std::string* error_message);

  // Checks if the referenced peer connection has an associated active
  // remote-bound log. If it does, the log is changed from ACTIVE to PENDING.
  void MaybeStopRemoteLogging(const PeerConnectionKey& key);

  // Get rid of pending logs whose age exceeds our retention policy.
  // On the one hand, we want to remove expired files as soon as possible, but
  // on the other hand, we don't want to waste CPU by checking this too often.
  // Therefore, we prune pending files:
  // 1. When a new BrowserContext is initalized, thereby also pruning the
  //    pending logs contributed by that BrowserContext.
  // 2. Before initiating a new upload, thereby avoiding uploading a file that
  //    has just now expired.
  // 3. On infrequent events - peer connection addition/removal, but NOT
  //    on something that could potentially be frequent, such as EventLogWrite.
  // Note that the last modification date of a file, which is the value measured
  // against for retention, is only read from disk once per file, meaning
  // this check is not too expensive.
  void PrunePendingLogs();

  // PrunePendingLogs() and schedule the next proactive prune.
  void RecurringPendingLogsPrune();

  // Removes pending logs files which match the given filter criteria, as
  // described by LogFileMatchesFilter's documentation.
  void MaybeRemovePendingLogs(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      base::Optional<BrowserContextId> browser_context_id =
          base::Optional<BrowserContextId>());

  // Cancels and deletes active logs which match the given filter criteria, as
  // described by LogFileMatchesFilter's documentation.
  void MaybeCancelActiveLogs(const base::Time& delete_begin,
                             const base::Time& delete_end,
                             BrowserContextId browser_context_id);

  // If the currently uploaded file matches the given filter criteria, as
  // described by LogFileMatchesFilter's documentation, the upload will be
  // cancelled, and the log file deleted. If this happens, the next pending log
  // file will be considered for upload.
  // This method is used to ensure that clearing of browsing data by the user
  // does not leave the currently-uploaded file on disk, even for the duration
  // of the upload.
  void MaybeCancelUpload(const base::Time& delete_begin,
                         const base::Time& delete_end,
                         base::Optional<BrowserContextId> browser_context_id =
                             base::Optional<BrowserContextId>());

  // Checks whether a log file matches a range and (potentially) BrowserContext:
  // * A file matches if its last modification date was at or later than
  //   |filter_range_begin|, and earlier than |filter_range_end|.
  // * If a null time-point is given as either |filter_range_begin| or
  //   |filter_range_end|, it is treated as "beginning-of-time" or
  //   "end-of-time", respectively.
  // * If |filter_browser_context_id| is set, only log files associated with it
  //   can match the filter.
  bool LogFileMatchesFilter(
      BrowserContextId log_browser_context_id,
      const base::Time& log_last_modification,
      base::Optional<BrowserContextId> filter_browser_context_id,
      const base::Time& filter_range_begin,
      const base::Time& filter_range_end) const;

  // Return |true| if and only if we can start another active log (with respect
  // to limitations on the numbers active and pending logs).
  bool AdditionalActiveLogAllowed(BrowserContextId browser_context_id) const;

  // Uploading suppressed while active peer connections exist (unless
  // suppression) is turned off from the command line.
  bool UploadSuppressed() const;

  // Check whether all the conditions necessary for uploading log files are
  // currently satisfied.
  // 1. There may be no active peer connections which might be adversely
  //    affected by the bandwidth consumption of the upload.
  // 2. Chrome has a network connection, and that conneciton is either a wired
  //    one, or WiFi. (That is, not 3G, etc.)
  // 3. Naturally, a file pending upload must exist.
  bool UploadConditionsHold() const;

  // When the conditions necessary for uploading first hold, schedule a delayed
  // task to upload (MaybeStartUploading). If they ever stop holding, void it.
  void ManageUploadSchedule();

  // Posted as a delayed task by ManageUploadSchedule. If not voided until
  // executed, will initiate an upload of the next log file.
  void MaybeStartUploading();

  // Callback for the success/failure of an upload.
  // When an upload is complete, it might be time to upload the next file.
  // Note: |log_file| and |upload_successful| are ignored in production; they
  // are used in unit tests, so we keep them here to make things simpler, so
  // that this method would match WebRtcEventLogUploader::UploadResultCallback
  // without adaptation.
  void OnWebRtcEventLogUploadComplete(const base::FilePath& log_file,
                                      bool upload_successful);

  // Given a renderer process ID and peer connection ID (a string naming the
  // peer connection), find the peer connection to which they refer.
  bool FindPeerConnection(int render_process_id,
                          const std::string& peer_connection_id,
                          PeerConnectionKey* key) const;

  // Find the next peer connection in a map to which the renderer process ID
  // and peer connection ID refer.
  // This helper allows FindPeerConnection() to DCHECK on uniqueness of the ID
  // without descending down a recursive rabbit hole.
  std::map<PeerConnectionKey, const std::string>::const_iterator
  FindNextPeerConnection(
      std::map<PeerConnectionKey, const std::string>::const_iterator begin,
      int render_process_id,
      const std::string& peer_connection_id) const;

  // Normally, uploading is suppressed while there are active peer connections.
  // This may be disabled from the command line.
  const bool upload_suppression_disabled_;

  // Proactive pruning will be done only if this is non-zero, in which case,
  // every |proactive_prune_scheduling_delta_|, pending logs will be pruned.
  // This avoids them staying around on disk for longer than their expiration
  // if no event occurs which triggers reactive pruning.
  const base::TimeDelta proactive_prune_scheduling_delta_;

  // The conditions for upload must hold for this much time, uninterrupted,
  // before an upload may be initiated.
  const base::TimeDelta upload_delay_;

  // Proactive pruning, if enabled, starts with the first enabled browser
  // context. To avoid unnecessary complexity, if that browser context is
  // disabled, proactive pruning is not disabled.
  bool proactive_prune_scheduling_started_;

  // This is used to inform WebRtcEventLogManager when remote-bound logging
  // of a peer connection starts/stops, which allows WebRtcEventLogManager to
  // decide when to ask WebRTC to start/stop sending event logs.
  WebRtcRemoteEventLogsObserver* const observer_;

  // The IDs of the BrowserContexts for which logging is enabled.
  std::set<BrowserContextId> enabled_browser_contexts_;

  // Currently active peer connections, mapped to their ID (as per the
  // RTCPeerConnection.id origin trial). PeerConnections which have been closed
  // are not considered active, regardless of whether they have been torn down.
  std::map<PeerConnectionKey, const std::string> active_peer_connections_;

  // Remote-bound logs which we're currently in the process of writing to disk.
  std::map<PeerConnectionKey, LogFile> active_logs_;

  // Remote-bound logs which have been written to disk before (either during
  // this Chrome session or during an earlier one), and which are no waiting to
  // be uploaded.
  std::set<WebRtcLogFileInfo> pending_logs_;

  // Null if no ongoing upload, or an uploader which owns a file, and is
  // currently busy uploading it to a remote server.
  std::unique_ptr<WebRtcEventLogUploader> uploader_;

  // Provides notifications of network changes.
  content::NetworkConnectionTracker* network_connection_tracker_;

  // Whether the network we are currently connected to, if any, is one over
  // which we may upload.
  bool uploading_supported_for_connection_type_;

  // If the conditions for initiating an upload do not hold, this will be
  // set to an empty base::TimeTicks.
  // If the conditions were found to hold, this will record the time when they
  // started holding. (It will be set back to 0 if they ever cease holding.)
  base::TimeTicks time_when_upload_conditions_met_;

  // This is a vehicle for DCHECKs to ensure code sanity. It counts the number
  // of scheduled tasks of MaybeStartUploading(), and proves that we never
  // end up with a scheduled upload that never occurs.
  size_t scheduled_upload_tasks_;

  // Producer of uploader objects. (In unit tests, this would create
  // null-implementation uploaders, or uploaders whose behavior is controlled
  // by the unit test.)
  std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory_;

  // |this| is created and destroyed on the UI thread, but operates on the
  // following IO-capable sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcRemoteEventLogManager);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_REMOTE_H_
