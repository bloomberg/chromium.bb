// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "content/common/content_export.h"

class WebrtcEventLogApiTest;

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
  static constexpr size_t kUnlimitedFileSize = 0;

  static WebRtcEventLogManager* GetInstance();

  // Currently, we only support manual logs initiated by the user
  // through WebRTCInternals, which are stored locally.
  // TODO(eladalon): Allow starting/stopping an RTC event log
  // that will be uploaded to the server. https://crbug.com/775415

  // Starts logging the WebRTC event logs associated with the given renderer's
  // process ID and the PeerConnection local ID, to a file whose path will
  // be derived from |base_path|, and potentially subject to a maximum size.
  // If the |reply| callback is non-empty, it will be posted back to the UI
  // thread with either the FilePath used for the log (if the operation was
  // successful), or with an empty FilePath (if the operation was unsuccessful).
  void LocalWebRtcEventLogStart(int render_process_id,
                                int lid,  // Renderer-local PeerConnection ID.
                                const base::FilePath& base_path,
                                size_t max_file_size_bytes,
                                base::OnceCallback<void(base::FilePath)> reply =
                                    base::OnceCallback<void(base::FilePath)>());

  // Stops an ongoing local WebRTC event log.
  // If |reply| is non-empty, it will be posted back to the UI thread, along
  // with true if there actually was a log to stop, or false otherwise.
  void LocalWebRtcEventLogStop(
      int render_process_id,
      int lid,
      base::OnceCallback<void(bool)> reply = base::OnceCallback<void(bool)>());

  // Called when a new log fragment is sent from the renderer. This will
  // potentially be written to a local WebRTC event log, a log destined for
  // upload, or both.
  void OnWebRtcEventLogWrite(
      int render_process_id,
      int lid,  // Renderer-local PeerConnection ID.
      const std::string& output,
      base::OnceCallback<void(bool)> reply = base::OnceCallback<void(bool)>());

 protected:
  friend class ::WebrtcEventLogApiTest;
  friend class WebRtcEventLogManagerTest;  // unit tests inject a frozen clock.
  // TODO(eladalon): Remove WebRtcEventLogHostTest and this friend declaration.
  // https://crbug.com/775415
  friend class WebRtcEventlogHostTest;
  friend struct base::LazyInstanceTraitsBase<WebRtcEventLogManager>;

  WebRtcEventLogManager();
  ~WebRtcEventLogManager();

  void InjectClockForTesting(base::Clock* clock);

  base::FilePath GetLocalFilePath(const base::FilePath& base_path,
                                  int render_process_id,
                                  int lid);

  // For a given Chrome session, this is a unique key for PeerConnections.
  // It's not, however, unique between sessions (after Chrome is restarted).
  struct PeerConnectionKey {
    bool operator<(const PeerConnectionKey& other) const {
      if (render_process_id != other.render_process_id) {
        return render_process_id < other.render_process_id;
      }
      return lid < other.lid;
    }
    int render_process_id;
    int lid;  // Renderer-local PeerConnection ID.
  };

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

  // Handling of local log files (local-user-initiated; as opposed to logs
  // requested by JS and bound to be uploaded to a remote server).
  void StartLocalLogFile(int render_process_id,
                         int lid,  // Renderer-local PeerConnection ID.
                         const base::FilePath& base_path,
                         size_t max_file_size_bytes,
                         base::OnceCallback<void(base::FilePath)> reply);
  void StopLocalLogFile(int render_process_id,
                        int lid,
                        base::OnceCallback<void(bool)> reply);
  void WriteToLocalLogFile(int render_process_id,
                           int lid,
                           const std::string& output,
                           base::OnceCallback<void(bool)> reply);
  void CloseLocalLogFile(LocalLogFilesMap::iterator it);  // Invalidates |it|.

  // For unit tests only, and specifically for unit tests that verify the
  // filename format (derived from the current time as well as the renderer PID
  // and PeerConnection local ID), we want to make sure that the time and date
  // cannot change between the time the clock is read by the unit under test
  // (namely WebRtcEventLogManager) and the time it's read by the test.
  base::Clock* clock_for_testing_;

  // Local log files, stored at the behest of the user (via WebRTCInternals).
  // TODO(eladalon): Add an additional container with logs that will be uploaded
  // to the server. https://crbug.com/775415
  LocalLogFilesMap local_logs_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcEventLogManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_RTC_EVENT_LOG_MANAGER_H_
