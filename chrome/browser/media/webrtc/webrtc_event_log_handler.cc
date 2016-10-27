// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/webrtc_log_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace {

// Returns a path name to be used as prefix for RTC event log files.
base::FilePath GetWebRtcEventLogPrefixPath(const base::FilePath& directory,
                                           uint64_t rtc_event_log_id) {
  static const char kWebRtcEventLogFilePrefix[] = "WebRtcEventLog.";
  return directory.AppendASCII(kWebRtcEventLogFilePrefix +
                               base::Uint64ToString(rtc_event_log_id));
}

}  // namespace

WebRtcEventLogHandler::WebRtcEventLogHandler(int render_process_id,
                                             Profile* profile)
    : render_process_id_(render_process_id),
      profile_(profile),
      current_rtc_event_log_id_(0) {
  DCHECK(profile_);
}

WebRtcEventLogHandler::~WebRtcEventLogHandler() {}

void WebRtcEventLogHandler::StartWebRtcEventLogging(
    base::TimeDelta duration,
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WebRtcEventLogHandler::GetLogDirectoryAndEnsureExists, this),
      base::Bind(&WebRtcEventLogHandler::DoStartWebRtcEventLogging, this,
                 duration, callback, error_callback));
}

void WebRtcEventLogHandler::StopWebRtcEventLogging(
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const bool is_manual_stop = true;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WebRtcEventLogHandler::GetLogDirectoryAndEnsureExists, this),
      base::Bind(&WebRtcEventLogHandler::DoStopWebRtcEventLogging, this,
                 is_manual_stop, current_rtc_event_log_id_, callback,
                 error_callback));
}

base::FilePath WebRtcEventLogHandler::GetLogDirectoryAndEnsureExists() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  base::FilePath log_dir_path =
      WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile_->GetPath());
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(log_dir_path, &error)) {
    DLOG(ERROR) << "Could not create WebRTC log directory, error: " << error;
    return base::FilePath();
  }
  return log_dir_path;
}

void WebRtcEventLogHandler::DoStartWebRtcEventLogging(
    base::TimeDelta duration,
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (!host) {
    error_callback.Run("RenderProcessHost not found");
    return;
  }

  base::FilePath prefix_path =
      GetWebRtcEventLogPrefixPath(log_directory, ++current_rtc_event_log_id_);
  if (!host->StartWebRTCEventLog(prefix_path)) {
    error_callback.Run("RTC event logging already in progress");
    return;
  }

  if (duration.is_zero()) {
    const bool is_stopped = false, is_manual_stop = false;
    callback.Run(prefix_path.AsUTF8Unsafe(), is_stopped, is_manual_stop);
    return;
  }

  const bool is_manual_stop = false;
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebRtcEventLogHandler::DoStopWebRtcEventLogging, this,
                 is_manual_stop, current_rtc_event_log_id_, callback,
                 error_callback, log_directory),
      duration);
}

void WebRtcEventLogHandler::DoStopWebRtcEventLogging(
    bool is_manual_stop,
    uint64_t rtc_event_log_id,
    const RecordingDoneCallback& callback,
    const RecordingErrorCallback& error_callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_LE(rtc_event_log_id, current_rtc_event_log_id_);

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (!host) {
    error_callback.Run("RenderProcessHost not found");
    return;
  }

  base::FilePath prefix_path =
      GetWebRtcEventLogPrefixPath(log_directory, rtc_event_log_id);
  // Prevent an old posted DoStopWebRtcEventLogging() call to stop a newer dump.
  // This could happen in a sequence like:
  //   Start(10);  // Start dump 1. Post Stop() to run after 10 seconds.
  //   Stop();     // Manually stop dump 1 before 10 seconds;
  //   Start(20);  // Start dump 2. Posted Stop() for 1 should not stop dump 2.
  if (rtc_event_log_id < current_rtc_event_log_id_) {
    const bool is_stopped = false;
    callback.Run(prefix_path.AsUTF8Unsafe(), is_stopped, is_manual_stop);
    return;
  }

  if (!host->StopWebRTCEventLog()) {
    error_callback.Run("No RTC event logging in progress");
    return;
  }

  const bool is_stopped = true;
  callback.Run(prefix_path.AsUTF8Unsafe(), is_stopped, is_manual_stop);
}
