// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_REMOTE_EVENT_LOG_MANAGER_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_REMOTE_EVENT_LOG_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/webrtc/webrtc_event_log_manager_common.h"
#include "content/browser/webrtc/webrtc_event_log_uploader.h"
#include "content/common/content_export.h"

namespace content {

// TODO(eladalon): Prevent uploading of logs when Chrome shutdown imminent.
// https://crbug.com/775415

class CONTENT_EXPORT WebRtcRemoteEventLogManager final
    : public LogFileWriter,
      public WebRtcEventLogUploaderObserver {
 public:
  explicit WebRtcRemoteEventLogManager(WebRtcRemoteEventLogsObserver* observer);
  ~WebRtcRemoteEventLogManager() override;

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
  bool PeerConnectionAdded(const PeerConnectionKey& key);
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
  // The return value is true if all of the restrictions were observed, and if
  // a file was successfully created for this log.
  bool StartRemoteLogging(const PeerConnectionKey& key,
                          const base::FilePath& browser_context_dir,
                          size_t max_file_size_bytes,
                          const std::string& metadata);

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

  // An implicit PeerConnectionRemoved() on all of the peer connections that
  // were associated with the renderer process.
  void RenderProcessHostExitedDestroyed(int render_process_id);

  // WebRtcEventLogUploaderObserver implementation.
  void OnWebRtcEventLogUploadComplete(const base::FilePath& file_path,
                                      bool upload_successful) override;

  // Unit tests may use this to inject null uploaders, or ones which are
  // directly controlled by the unit test (succeed or fail according to the
  // test's needs).
  // Note that for simplicity's sake, this may be called from outside the
  // task queue on which this object lives (WebRtcEventLogManager::task_queue_).
  // Therefore, if a test calls this, it should call it before it initializes
  // any BrowserContext with pending log files in its directory.
  void SetWebRtcEventLogUploaderFactoryForTesting(
      std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory);

 protected:
  friend class WebRtcEventLogManagerTest;

  // Given a BrowserContext's directory, return the path to the directory where
  // we store the pending remote-bound logs associated with this BrowserContext.
  static base::FilePath GetLogsDirectoryPath(
      const base::FilePath& browser_context_dir);

 private:
  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;

  struct PendingLog {
    PendingLog(BrowserContextId browser_context_id,
               const base::FilePath& path,
               base::Time last_modified)
        : browser_context_id(browser_context_id),
          path(path),
          last_modified(last_modified) {}

    bool operator<(const PendingLog& other) const {
      if (last_modified != other.last_modified) {
        return last_modified < other.last_modified;
      }
      return path < other.path;  // Break ties arbitrarily, but consistently.
    }

    const BrowserContextId browser_context_id;  // This file's owner.
    const base::FilePath path;
    // |last_modified| recorded at BrowserContext initialization. Chrome will
    // not modify it afterwards, and neither should the user.
    const base::Time last_modified;
  };

  // Checks whether a browser context has already been enabled via a call to
  // EnableForBrowserContext(), and not yet disabled using a call to
  // DisableForBrowserContext().
  bool BrowserContextEnabled(BrowserContextId browser_context_id) const;

  // LogFileWriter implementation. Closes an active log file, changing its
  // state from ACTIVE to PENDING.
  LogFilesMap::iterator CloseLogFile(LogFilesMap::iterator it) override;

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
  // log may be written. True is returned if and only if such a file was
  // successfully created.
  bool StartWritingLog(const PeerConnectionKey& key,
                       const base::FilePath& browser_context_dir,
                       size_t max_file_size_bytes,
                       const std::string& metadata);

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

  // Return |true| if and only if we can start another active log (with respect
  // to limitations on the numbers active and pending logs).
  bool AdditionalActiveLogAllowed(BrowserContextId browser_context_id) const;

  // Initiating a new upload is only allowed when there are no active peer
  // connection which might be adversely affected by the bandwidth consumption
  // of the upload.
  // TODO(eladalon): Add support for pausing/resuming an upload when peer
  // connections are added/removed after an upload was already initiated.
  // https://crbug.com/775415
  bool UploadingAllowed() const;

  // If no upload is in progress, and if uploading is currently permissible,
  // start a new upload.
  void MaybeStartUploading();

  // When an upload is complete, it might be time to upload the next file.
  void OnWebRtcEventLogUploadCompleteInternal();

  // This object is expected to be created and destroyed on the UI thread,
  // but live on its owner's internal, IO-capable task queue.
  SEQUENCE_CHECKER(io_task_sequence_checker_);

  // This is used to inform WebRtcEventLogManager when remote-bound logging
  // of a peer connection starts/stops, which allows WebRtcEventLogManager to
  // decide when to ask WebRTC to start/stop sending event logs.
  WebRtcRemoteEventLogsObserver* const observer_;

  // The IDs of the BrowserContexts for which logging is enabled.
  std::set<BrowserContextId> enabled_browser_contexts_;

  // Currently active peer connections. PeerConnections which have been closed
  // are not considered active, regardless of whether they have been torn down.
  std::set<PeerConnectionKey> active_peer_connections_;

  // Remote-bound logs which we're currently in the process of writing to disk.
  std::map<PeerConnectionKey, LogFile> active_logs_;

  // Remote-bound logs which have been written to disk before (either during
  // this Chrome session or during an earlier one), and which are no waiting to
  // be uploaded.
  std::set<PendingLog> pending_logs_;

  // Null if no ongoing upload, or an uploader which owns a file, and is
  // currently busy uploading it to a remote server.
  std::unique_ptr<WebRtcEventLogUploader> uploader_;

  // Producer of uploader objects. (In unit tests, this would create
  // null-implementation uploaders, or uploaders whose behavior is controlled
  // by the unit test.)
  std::unique_ptr<WebRtcEventLogUploader::Factory> uploader_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcRemoteEventLogManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_REMOTE_EVENT_LOG_MANAGER_H_
