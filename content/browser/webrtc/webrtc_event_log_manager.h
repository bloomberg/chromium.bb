// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_H_

#include <map>
#include <type_traits>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "content/browser/webrtc/webrtc_event_log_manager_common.h"
#include "content/browser/webrtc/webrtc_local_event_log_manager.h"
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
class CONTENT_EXPORT WebRtcEventLogManager
    : protected WebRtcLocalEventLogsObserver {
 public:
  // Ensures that no previous instantiation of the class was performed, then
  // instantiates the class and returns the object. Subsequent calls to
  // GetInstance() will return this object.
  static WebRtcEventLogManager* CreateSingletonInstance();

  // Returns the object previously constructed using CreateSingletonInstance().
  // Can be null in tests.
  static WebRtcEventLogManager* GetInstance();

  ~WebRtcEventLogManager() override;

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
  void SetLocalLogsObserver(WebRtcLocalEventLogsObserver* observer,
                            base::OnceClosure reply = base::OnceClosure());

 protected:
  friend class WebRtcEventLogManagerTest;  // Unit tests inject a frozen clock.

  WebRtcEventLogManager();

  void SetTaskRunnerForTesting(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

 private:
  using PeerConnectionKey = WebRtcEventLogPeerConnectionKey;

  // This bitmap allows us to track for which clients (local/remote logging)
  // we have turned WebRTC event logging on for a given peer connection, so that
  // we may turn it off only when the last client no longer needs it.
  enum LoggingTarget : unsigned int {
    kLocalLogging = 0x01
    // TODO(eladalon): Add kRemoteLogging as 0x02. https://crbug.com/775415
  };
  using LoggingTargetBitmap = std::underlying_type<LoggingTarget>::type;

  static WebRtcEventLogManager* g_webrtc_event_log_manager;

  // WebRtcLocalEventLogsObserver implementation:
  void OnLocalLogStarted(PeerConnectionKey peer_connection,
                         base::FilePath file_path) override;
  void OnLocalLogStopped(PeerConnectionKey peer_connection) override;

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

  void OnWebRtcEventLogWriteInternal(
      int render_process_id,
      int lid,  // Renderer-local PeerConnection ID.
      const std::string& output,
      base::OnceCallback<void(bool)> reply);

  void SetLocalLogsObserverInternal(WebRtcLocalEventLogsObserver* observer,
                                    base::OnceClosure reply);

  // Send a message to WebRTC telling it to start/stop sending event-log
  // notifications for a given peer connection.
  void UpdateWebRtcEventLoggingState(PeerConnectionKey peer_connection,
                                     bool enabled);

  void InjectClockForTesting(base::Clock* clock);

  // Observer which will be informed whenever a local log file is started or
  // stopped. Its callbacks are called synchronously from |task_runner_|,
  // so the observer needs to be able to either run from any (sequenced) runner.
  WebRtcLocalEventLogsObserver* local_logs_observer_;

  // Manages local-bound logs - logs stored on the local filesystem when
  // logging has been explicitly enabled by the user.
  WebRtcLocalEventLogManager local_logs_manager_;

  // This keeps track of which peer connections have event logging turned on
  // in WebRTC, and for which client(s).
  std::map<PeerConnectionKey, LoggingTargetBitmap>
      peer_connections_with_event_logging_enabled_;

  // The main logic will run sequentially on this runner, on which blocking
  // tasks are allowed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcEventLogManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_H_
