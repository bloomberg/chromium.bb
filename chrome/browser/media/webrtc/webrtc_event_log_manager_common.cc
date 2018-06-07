// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

#include <limits>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;

const char kStartRemoteLoggingFailureFeatureDisabled[] = "Feature disabled.";
const char kStartRemoteLoggingFailureUnlimitedSizeDisallowed[] =
    "Unlimited size disallowed.";
const char kStartRemoteLoggingFailureMaxSizeTooLarge[] =
    "Excessively large max log size.";
const char kStartRemoteLoggingFailureMetadaTooLong[] =
    "Excessively long metadata.";
const char kStartRemoteLoggingFailureMaxSizeTooSmall[] = "Max size too small.";
const char kStartRemoteLoggingFailureUnknownOrInactivePeerConnection[] =
    "Unknown or inactive peer connection.";
const char kStartRemoteLoggingFailureAlreadyLogging[] = "Already logging.";
const char kStartRemoteLoggingFailureGeneric[] = "Unspecified error.";

const BrowserContextId kNullBrowserContextId =
    reinterpret_cast<BrowserContextId>(nullptr);

bool LogFileWriter::WriteToLogFile(LogFilesMap::iterator it,
                                   const std::string& message) {
  DCHECK_LE(message.length(),
            static_cast<size_t>(std::numeric_limits<int>::max()));

  // Observe the file size limit, if any. Note that base::File's interface does
  // not allow writing more than numeric_limits<int>::max() bytes at a time.
  int message_len = static_cast<int>(message.length());  // DCHECKed above.
  LogFile& log_file = it->second;
  if (log_file.max_file_size_bytes != kWebRtcEventLogManagerUnlimitedFileSize) {
    DCHECK_LT(log_file.file_size_bytes, log_file.max_file_size_bytes);
    const bool size_will_wrap_around =
        log_file.file_size_bytes + message.length() < log_file.file_size_bytes;
    const bool size_limit_will_be_exceeded =
        log_file.file_size_bytes + message.length() >
        log_file.max_file_size_bytes;
    if (size_will_wrap_around || size_limit_will_be_exceeded) {
      CloseLogFile(it);
      return false;
    }
  }

  int written = log_file.file.WriteAtCurrentPos(message.c_str(), message_len);
  if (written != message_len) {
    LOG(WARNING) << "WebRTC event log message couldn't be written to the "
                    "locally stored file in its entirety.";
    CloseLogFile(it);
    return false;
  }

  log_file.file_size_bytes += static_cast<size_t>(written);
  if (log_file.max_file_size_bytes != kWebRtcEventLogManagerUnlimitedFileSize) {
    DCHECK_LE(log_file.file_size_bytes, log_file.max_file_size_bytes);
    if (log_file.file_size_bytes >= log_file.max_file_size_bytes) {
      CloseLogFile(it);
    }
  }

  return (static_cast<size_t>(written) == message.length());
}

BrowserContextId GetBrowserContextId(
    const content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return reinterpret_cast<BrowserContextId>(browser_context);
}

BrowserContextId GetBrowserContextId(int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* const host =
      content::RenderProcessHost::FromID(render_process_id);

  content::BrowserContext* const browser_context =
      host ? host->GetBrowserContext() : nullptr;

  return GetBrowserContextId(browser_context);
}

base::FilePath GetRemoteBoundWebRtcEventLogsDir(
    const base::FilePath& browser_context_dir) {
  const base::FilePath::CharType kRemoteBoundLogSubDirectory[] =
      FILE_PATH_LITERAL("webrtc_event_logs");
  return browser_context_dir.Append(kRemoteBoundLogSubDirectory);
}
