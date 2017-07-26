// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender_pipe_win.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

MemlogSenderPipe::MemlogSenderPipe(mojo::edk::ScopedPlatformHandle handle)
    : handle_(std::move(handle)) {}

MemlogSenderPipe::~MemlogSenderPipe() {
}

bool MemlogSenderPipe::Send(const void* data, size_t sz) {
  // The pipe uses a blocking wait mode (it doesn't specify PIPE_NOWAIT) so
  // WriteFile should do only complete writes.
  //
  // Note: don't use logging here (CHECK, DCHECK) because they will allocate,
  // and this function is called from within a malloc hook.
  DWORD bytes_written = 0;
  if (!::WriteFile(handle_.get().handle, data, static_cast<DWORD>(sz),
                   &bytes_written, NULL))
    return false;
  return true;
}

}  // namespace profiling
