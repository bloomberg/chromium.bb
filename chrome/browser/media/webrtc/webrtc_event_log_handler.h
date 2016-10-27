// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_HANDLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

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

  WebRtcEventLogHandler(int render_process_id, Profile* profile);

  // Starts an RTC event log for each peerconnection on this RenderProcessHost.
  // The call writes the most recent events and all future events for |duration|
  // seconds to a file. After |duration|, recording stops and |callback| is
  // invoked. If |duration| is zero, |callback| is run immediately and the
  // logging will continue until StopWebRtcEventLogging() is explicitly invoked.
  // Must be called on the UI thread.
  void StartWebRtcEventLogging(base::TimeDelta duration,
                               const RecordingDoneCallback& callback,
                               const RecordingErrorCallback& error_callback);

  // Stops an RTC event log. Must be called on the UI thread.
  // |callback| is invoked once recording stops. If no recording was in
  // progress, |error_callback| is invoked instead of |callback|.
  void StopWebRtcEventLogging(const RecordingDoneCallback& callback,
                              const RecordingErrorCallback& error_callback);

 private:
  friend class base::RefCountedThreadSafe<WebRtcEventLogHandler>;
  virtual ~WebRtcEventLogHandler();

  base::FilePath GetLogDirectoryAndEnsureExists();

  // Helper for starting RTC event logs.
  void DoStartWebRtcEventLogging(base::TimeDelta duration,
                                 const RecordingDoneCallback& callback,
                                 const RecordingErrorCallback& error_callback,
                                 const base::FilePath& log_directory);

  // Helper for stopping RTC event logs.
  void DoStopWebRtcEventLogging(bool is_manual_stop,
                                uint64_t rtc_event_log_id,
                                const RecordingDoneCallback& callback,
                                const RecordingErrorCallback& error_callback,
                                const base::FilePath& log_directory);

  // The ID of our render process.
  int render_process_id_;

  // The profile associated with our renderer process.
  const Profile* const profile_;

  // This counter allows saving each log in a separate file.
  uint64_t current_rtc_event_log_id_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcEventLogHandler);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_HANDLER_H_
