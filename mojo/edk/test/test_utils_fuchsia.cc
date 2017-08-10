// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/test_utils.h"

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace mojo {
namespace edk {
namespace test {

// TODO(fuchsia): Add file-descriptor support to PlatformHandle and implement
// these, merge them with the POSIX impl, or remove them, as appropriate.
// See crbug.com/754029.

bool BlockingWrite(const PlatformHandle& handle,
                   const void* buffer,
                   size_t bytes_to_write,
                   size_t* bytes_written) {
  NOTIMPLEMENTED();
  return false;
}

bool BlockingRead(const PlatformHandle& handle,
                  void* buffer,
                  size_t buffer_size,
                  size_t* bytes_read) {
  NOTIMPLEMENTED();
  return false;
}

bool NonBlockingRead(const PlatformHandle& handle,
                     void* buffer,
                     size_t buffer_size,
                     size_t* bytes_read) {
  NOTIMPLEMENTED();
  return false;
}

ScopedPlatformHandle PlatformHandleFromFILE(base::ScopedFILE fp) {
  NOTIMPLEMENTED();
  return ScopedPlatformHandle(PlatformHandle());
}

base::ScopedFILE FILEFromPlatformHandle(ScopedPlatformHandle h,
                                        const char* mode) {
  NOTIMPLEMENTED();
  return base::ScopedFILE();
}

}  // namespace test
}  // namespace edk
}  // namespace mojo
