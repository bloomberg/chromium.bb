// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_unittest_helpers.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

// Produce a LogFileWriter::Factory object.
std::unique_ptr<LogFileWriter::Factory> CreateLogFileWriterFactory(
    WebRtcEventLogCompression compression) {
  switch (compression) {
    case WebRtcEventLogCompression::NONE:
      return std::make_unique<BaseLogFileWriterFactory>();
  }

  NOTREACHED();
  return nullptr;  // Appease compiler.
}

#if defined(OS_POSIX)
void RemoveWritePermissions(const base::FilePath& path) {
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &permissions));
  constexpr int write_permissions = base::FILE_PERMISSION_WRITE_BY_USER |
                                    base::FILE_PERMISSION_WRITE_BY_GROUP |
                                    base::FILE_PERMISSION_WRITE_BY_OTHERS;
  permissions &= ~write_permissions;
  ASSERT_TRUE(base::SetPosixFilePermissions(path, permissions));
}
#endif  // defined(OS_POSIX)
