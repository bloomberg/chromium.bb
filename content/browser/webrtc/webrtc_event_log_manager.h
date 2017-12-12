// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <tuple>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

// This is a singleton class running in the browser UI thread.
// It is in charge of writing RTC event logs to temporary files, then uploading
// those files to remote servers, as well as of writing the logs to files which
// were manually indicated by the user from the WebRTCIntenals. (A log may
// simulatenously be written to both, either, or none.)
// TODO(eladalon): This currently only supports the old use-case - locally
// stored log files. An upcoming CL will add remote-support.
// https://crbug.com/775415
class CONTENT_EXPORT WebRtcEventLogManager {
 public:
  // For a given Chrome session, this is a unique key for PeerConnections.
  // It's not, however, unique between sessions (after Chrome is restarted).
  struct PeerConnectionKey {
    constexpr PeerConnectionKey(int render_process_id, int lid)
        : render_process_id(render_process_id), lid(lid) {}

    bool operator==(const PeerConnectionKey& other) const {
      return std::tie(render_process_id, lid) ==
             std::tie(other.render_process_id, other.lid);
    }

    bool operator<(const PeerConnectionKey& other) const {
      return std::tie(render_process_id, lid) <
             std::tie(other.render_process_id, other.lid);
    }

    int render_process_id;
    int lid;  // Renderer-local PeerConnection ID.
  };

  // Allow an observer to be registered for notifications of local log files
  // being started/stopped, and the paths which will be used for these logs.
  class LocalLogsObserver {
   public:
    virtual ~LocalLogsObserver() = default;
    virtual void OnLocalLogsStarted(PeerConnectionKey peer_connection,
                                    base::FilePath file_path) = 0;
    virtual void OnLocalLogsStopped(PeerConnectionKey peer_connection) = 0;
  };

  static constexpr size_t kUnlimitedFileSize = 0;

#if defined(OS_ANDROID)
  static const size_t kMaxNumberLocalWebRtcEventLogFiles = 3;
  static const size_t kDefaultMaxLocalLogFileSizeBytes = 10000000;
#else
  static const size_t kMaxNumberLocalWebRtcEventLogFiles = 5;
  static const size_t kDefaultMaxLocalLogFileSizeBytes = 60000000;
#endif

  static WebRtcEventLogManager* GetInstance();

  // Currently, we only support manual logs initiated by the user
  // through WebRTCInternals, which are stored locally.
  // TODO(eladalon): Allow starting/stopping an RTC event log
  // that will be uploaded to the server. https://crbug.com/775415

  // Call this to let the manager know when a PeerConnection was created.
  // If a reply callback is given, it will be posted back to BrowserThread::UI,
  // with true if and only if the operation was successful (failure is only
  // possible if a peer connection with this exact key was previously added,
  // but not removed).
  void PeerConnectionAdded(
      int render_process_id,
      int lid,  // Renderer-local PeerConnection ID.
      base::OnceCallback<void(bool)> reply = base::OnceCallback<void(bool)>());

  // Call this to let the manager know when a PeerConnection was closed.
  // If a reply callback is given, it will be posted back to BrowserThread::UI,
  // with true if and only if the operation was successful (failure is only
  // possible if a peer connection with this key was not previously added,
  // or if it has since already been removed).
  void PeerConnectionRemoved(
      int render_process_id,
      int lid,  // Renderer-local PeerConnection ID.
      base::OnceCallback<void(bool)> reply = base::OnceCallback<void(bool)>());

  // Enable local logging of RTC events.
  // Local logging is distinguished from remote logging, in that local logs are
  // kept in response to explicit user input, are saved to a specific location,
  // and are never removed by Chrome.
  // The file's actual path is derived from |base_path| by adding a timestamp,
  // the render process ID and the PeerConnection's local ID.
  // If a reply callback is given, it will be posted back to BrowserThread::UI,
  // with true if and only if local logging was *not* already on.
  // Note #1: An illegal file path, or one where we don't have the necessary
  // permissions, does not cause a |false| reply, since even when we have the
  // permissions, we're not guaranteed to keep them, and some files might be
  // legal while others aren't due to additional restrictions (number of files,
  // length of filename, etc.).
  // Note #2: If the number of currently active peer connections exceeds the
  // maximum number of local log files, there is no guarantee about which PCs
  // will get a local log file associated (specifically, we do *not* guarantee
  // it would be either the oldest or the newest).
  void EnableLocalLogging(
      base::FilePath base_path,
      size_t max_file_size_bytes = kDefaultMaxLocalLogFileSizeBytes,
      base::OnceCallback<void(bool)> reply = base::OnceCallback<void(bool)>());

  // Disable local logging of RTC events.
  // Any active local logs are stopped. Peer connections added after this call
  // will not get a local log associated with them.
  void DisableLocalLogging(
      base::OnceCallback<void(bool)> reply = base::OnceCallback<void(bool)>());

  // Called when a new log fragment is sent from the renderer. This will
  // potentially be written to a local WebRTC event log, a log destined for
  // upload, or both.
  // If a reply callback is given, it will be posted back to BrowserThread::UI,
  // with true if and only if |output| was written in its entirety to both the
  // local log (if any) as well as the remote log (if any). In the edge case
  // that neither log file exists, false will be returned.
  void OnWebRtcEventLogWrite(
      int render_process_id,
      int lid,  // Renderer-local PeerConnection ID.
      const std::string& output,
      base::OnceCallback<void(bool)> reply = base::OnceCallback<void(bool)>());

  // Set (or unset) an observer that will be informed whenever a local log file
  // is started/stopped. The observer needs to be able to either run from
  // anywhere. If you need the code to run on specific runners or queues, have
  // the observer post them there.
  // If a reply callback is given, it will be posted back to BrowserThread::UI
  // after the observer has been set.
  void SetLocalLogsObserver(LocalLogsObserver* observer,
                            base::OnceClosure reply = base::OnceClosure());

 protected:
  friend class WebRtcEventLogManagerTest;  // unit tests inject a frozen clock.
  friend struct base::LazyInstanceTraitsBase<WebRtcEventLogManager>;

  struct LogFile {
    LogFile(base::File file, size_t max_file_size_bytes)
        : file(std::move(file)),
          max_file_size_bytes(max_file_size_bytes),
          file_size_bytes(0) {}
    base::File file;
    const size_t max_file_size_bytes;
    size_t file_size_bytes;
  };

  typedef std::map<PeerConnectionKey, LogFile> LocalLogFilesMap;

  WebRtcEventLogManager();
  virtual ~WebRtcEventLogManager();

  void PeerConnectionAddedInternal(int render_process_id,
                                   int lid,
                                   base::OnceCallback<void(bool)> reply);

  void PeerConnectionRemovedInternal(int render_process_id,
                                     int lid,
                                     base::OnceCallback<void(bool)> reply);

  void EnableLocalLoggingInternal(base::FilePath base_path,
                                  size_t max_file_size_bytes,
                                  base::OnceCallback<void(bool)> reply);

  void DisableLocalLoggingInternal(base::OnceCallback<void(bool)> reply);

  void SetLocalLogsObserverInternal(LocalLogsObserver* observer,
                                    base::OnceClosure reply);

  // Local log file handling.
  void StartLocalLogFile(int render_process_id, int lid);
  void StopLocalLogFile(int render_process_id, int lid);
  void WriteToLocalLogFile(int render_process_id,
                           int lid,
                           const std::string& output,
                           base::OnceCallback<void(bool)> reply);
  LocalLogFilesMap::iterator CloseLocalLogFile(LocalLogFilesMap::iterator it);

  // Determine whether WebRTC state needs to be updated for the given peer
  // connection, and if so, sends a message back to the UI process to do so.
  void MaybeUpdateWebRtcEventLoggingState(int render_process_id, int lid);

  // Determine whether WebRTC state needs to be updated for the given peer
  // connection, and if so, sends a message back to the UI process to do so.
  void UpdateWebRtcEventLoggingState(int render_process_id,
                                     int lid,
                                     bool enabled);

  // Derives the name of a local log file. The format is:
  // [user_defined]_[date]_[time]_[pid]_[lid].log
  base::FilePath GetLocalFilePath(const base::FilePath& base_path,
                                  int render_process_id,
                                  int lid);

  // Only used for testing, so we have no threading concenrs here; it should
  // always be called before anything that might post to the internal TQ.
  void InjectClockForTesting(base::Clock* clock);

  // For unit tests only, and specifically for unit tests that verify the
  // filename format (derived from the current time as well as the renderer PID
  // and PeerConnection local ID), we want to make sure that the time and date
  // cannot change between the time the clock is read by the unit under test
  // (namely WebRtcEventLogManager) and the time it's read by the test.
  base::Clock* clock_for_testing_;

  // Currently active peer connections. PeerConnections which have been closed
  // are not considered active, regardless of whether they have been torn down.
  std::set<PeerConnectionKey> active_peer_connections_;

  // Local log files, stored at the behest of the user (via WebRTCInternals).
  // TODO(eladalon): Add an additional container with logs that will be uploaded
  // to the server. https://crbug.com/775415
  LocalLogFilesMap local_logs_;

  // Observer which will be informed whenever a local log file is started or
  // stopped. Its callbacks are called synchronously from |file_task_runner_|,
  // so the observer needs to be able to either run from any (sequenced) runner.
  LocalLogsObserver* local_logs_observer_;

  // If FilePath is empty, local logging is disabled.
  // If nonempty, local logging is enabled, and all local logs will be saved
  // to this directory.
  base::FilePath local_logs_base_path_;

  // The maximum size for local logs, in bytes. Note that kUnlimitedFileSize is
  // a sentinel value (with a self-explanatory name).
  size_t max_local_log_file_size_bytes_;

  // File operations will run sequentially on this runner.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcEventLogManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_H_
