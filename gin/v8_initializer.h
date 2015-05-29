// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_INITIALIZER_H_
#define GIN_V8_INITIALIZER_H_

#include "base/files/file.h"
#include "gin/array_buffer.h"
#include "gin/gin_export.h"
#include "gin/public/isolate_holder.h"
#include "gin/public/v8_platform.h"
#include "v8/include/v8.h"

namespace gin {

class GIN_EXPORT V8Initializer {
 public:
  // This should be called by IsolateHolder::Initialize().
  static void Initialize(gin::IsolateHolder::ScriptMode mode);

  // Get address and size information for currently loaded snapshot.
  // If no snapshot is loaded, the return values are null for addresses
  // and 0 for sizes.
  static void GetV8ExternalSnapshotData(const char** natives_data_out,
                                        int* natives_size_out,
                                        const char** snapshot_data_out,
                                        int* snapshot_size_out);

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)

  // Load V8 snapshot from user provided platform file descriptors.
  // The offset and size arguments, if non-zero, specify the portions
  // of the files to be loaded. This methods returns true on success
  // (or if snapshot is already loaded), false otherwise.
  static bool LoadV8SnapshotFromFD(base::PlatformFile natives_fd,
                                   int64 natives_offset,
                                   int64 natives_size,
                                   base::PlatformFile snapshot_fd,
                                   int64 snapshot_offset,
                                   int64 snapshot_size);

  // Load V8 snapshot from default resources. Returns true on success or
  // snapshot is already loaded, false otherwise.
  static bool LoadV8Snapshot();

  // Opens the V8 snapshot data files and returns open file descriptors to these
  // files in |natives_fd_out| and |snapshot_fd_out|, which can be passed to
  // child processes.
  static bool OpenV8FilesForChildProcesses(base::PlatformFile* natives_fd_out,
                                           base::PlatformFile* snapshot_fd_out);

#endif  // V8_USE_EXTERNAL_STARTUP_DATA
};

}  // namespace gin

#endif  // GIN_V8_INITIALIZER_H_
