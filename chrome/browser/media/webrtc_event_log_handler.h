// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_EVENT_LOG_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_EVENT_LOG_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace content {
class RenderProcessHost;
}  // namespace content
class Profile;

// WebRtcEventLogHandler provides an interface to start and stop
// the WebRTC event log.
class WebRtcEventLogHandler
    : public base::RefCountedThreadSafe<WebRtcEventLogHandler> {
 public:
  typedef base::Callback<void(bool, const std::string&)> GenericDoneCallback;
  typedef base::Callback<void(const std::string&)> RecordingErrorCallback;
  typedef base::Callback<void(const std::string&, bool, bool)>
      RecordingDoneCallback;

  // Key used to attach the handler to the RenderProcessHost.
  static const char kWebRtcEventLogHandlerKey[];

  explicit WebRtcEventLogHandler(Profile* profile);

  // Starts an RTC event log. The call writes the most recent events to a
  // file and then starts logging events for the given |delay|.
  // If |delay| is zero, the logging will continue until
  // StopWebRtcEventLogging()
  // is explicitly invoked.
  // |callback| is invoked once recording stops. If |delay| is zero
  // |callback| is invoked once recording starts.
  // If a recording was already in progress, |error_callback| is invoked instead
  // of |callback|.
  void StartWebRtcEventLogging(content::RenderProcessHost* host,
                               base::TimeDelta delay,
                               const RecordingDoneCallback& callback,
                               const RecordingErrorCallback& error_callback);

  // Stops an RTC event log. |callback| is invoked once recording
  // stops. If no recording was in progress, |error_callback| is invoked instead
  // of |callback|.
  void StopWebRtcEventLogging(content::RenderProcessHost* host,
                              const RecordingDoneCallback& callback,
                              const RecordingErrorCallback& error_callback);

 private:
  friend class base::RefCountedThreadSafe<WebRtcEventLogHandler>;
  virtual ~WebRtcEventLogHandler();

  base::FilePath GetLogDirectoryAndEnsureExists();

  // Helper for starting RTC event logs.
  void DoStartWebRtcEventLogging(content::RenderProcessHost* host,
                                 base::TimeDelta delay,
                                 const RecordingDoneCallback& callback,
                                 const RecordingErrorCallback& error_callback,
                                 const base::FilePath& log_directory);

  // Helper for stopping RTC event logs.
  void DoStopWebRtcEventLogging(content::RenderProcessHost* host,
                                bool is_manual_stop,
                                uint64_t audio_debug_recordings_id,
                                const RecordingDoneCallback& callback,
                                const RecordingErrorCallback& error_callback,
                                const base::FilePath& log_directory);

  // The profile associated with our renderer process.
  Profile* const profile_;

  // Must be accessed on the UI thread.
  bool is_rtc_event_logging_in_progress_;

  // This counter allows saving each log in a separate file.
  uint64_t current_rtc_event_log_id_;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(WebRtcEventLogHandler);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_EVENT_LOG_HANDLER_H_
