// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/test_utils.h"

#include <windows.h>

#include "mojo/system/embedder/platform_handle.h"

namespace mojo {
namespace test {

bool BlockingWrite(const embedder::PlatformHandle& handle,
                   const void* buffer,
                   size_t bytes_to_write,
                   size_t* bytes_written) {
  OVERLAPPED overlapped = { 0 };
  DWORD bytes_written_dword = 0;

  if (!WriteFile(handle.handle, buffer, bytes_to_write, &bytes_written_dword,
                 &overlapped)) {
    if (GetLastError() != ERROR_IO_PENDING ||
        !GetOverlappedResult(handle.handle, &overlapped, &bytes_written_dword,
                             TRUE)) {
      return false;
    }
  }

  *bytes_written = bytes_written_dword;
  return true;
}

bool BlockingRead(const embedder::PlatformHandle& handle,
                  void* buffer,
                  size_t buffer_size,
                  size_t* bytes_read) {
  OVERLAPPED overlapped = { 0 };
  DWORD bytes_read_dword = 0;

  if (!ReadFile(handle.handle, buffer, buffer_size, &bytes_read_dword,
                &overlapped)) {
    if (GetLastError() != ERROR_IO_PENDING ||
        !GetOverlappedResult(handle.handle, &overlapped, &bytes_read_dword,
                             TRUE)) {
      return false;
    }
  }

  *bytes_read = bytes_read_dword;
  return true;
}

bool NonBlockingRead(const embedder::PlatformHandle& handle,
                     void* buffer,
                     size_t buffer_size,
                     size_t* bytes_read) {
  OVERLAPPED overlapped = { 0 };
  DWORD bytes_read_dword = 0;

  if (!ReadFile(handle.handle, buffer, buffer_size, &bytes_read_dword,
                &overlapped)) {
    if (GetLastError() != ERROR_IO_PENDING)
      return false;

    CancelIo(handle.handle);

    if (!GetOverlappedResult(handle.handle, &overlapped, &bytes_read_dword,
                             TRUE)) {
      *bytes_read = 0;
      return true;
    }
  }

  *bytes_read = bytes_read_dword;
  return true;
}

}  // namespace test
}  // namespace mojo
