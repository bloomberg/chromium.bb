// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender_pipe_posix.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/profiling/memlog_stream.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"

namespace profiling {

MemlogSenderPipe::MemlogSenderPipe(const std::string& pipe_id) {
  int fd;
  CHECK(base::StringToInt(pipe_id, &fd));
  handle_.reset(mojo::edk::PlatformHandle(fd));
}

MemlogSenderPipe::~MemlogSenderPipe() {
}

bool MemlogSenderPipe::Connect() {
  // In posix-land, the pipe is just handed to us already connected.
  return true;
}

bool MemlogSenderPipe::Send(const void* data, size_t sz) {
  return mojo::edk::PlatformChannelWrite(handle_.get(), data, sz);
}

}  // namespace profiling
