// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_UNITTEST_HELPERS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_UNITTEST_HELPERS_H_

#include <memory>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

// Which type of compression, if any, LogFileWriterTest should use.
enum class WebRtcEventLogCompression {
  NONE
  // TODO(crbug.com/775415): Add support for GZIP.
};

// Produce a LogFileWriter::Factory object.
std::unique_ptr<LogFileWriter::Factory> CreateLogFileWriterFactory(
    WebRtcEventLogCompression compression);

#if defined(OS_POSIX)
void RemoveWritePermissions(const base::FilePath& path);
#endif  // defined(OS_POSIX)

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_UNITTEST_HELPERS_H_
