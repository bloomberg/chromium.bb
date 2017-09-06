// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender_pipe_posix.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/profiling/memlog_stream.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"

namespace profiling {

MemlogSenderPipe::MemlogSenderPipe(base::ScopedPlatformFile file)
    : handle_(mojo::edk::PlatformHandle(file.release())) {}

MemlogSenderPipe::~MemlogSenderPipe() {
}

bool MemlogSenderPipe::Connect() {
  // In posix-land, the pipe is just handed to us already connected.
  return true;
}

bool MemlogSenderPipe::Send(const void* data, size_t sz) {
  base::AutoLock lock(lock_);
  int size = static_cast<int>(sz);
  while (size > 0) {
    int r = mojo::edk::PlatformChannelWrite(handle_.get(), data, size);
    if (r == -1)
      return false;
    DCHECK_LE(r, size);
    size -= r;
    data = static_cast<const char*>(data) + r;
  }
  return true;
}

}  // namespace profiling
