// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender_pipe_posix.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

MemlogSenderPipe::MemlogSenderPipe(const std::string& pipe_id)
    : pipe_id_(pipe_id), fd_(-1) {}

MemlogSenderPipe::~MemlogSenderPipe() {
  if (fd_ != -1)
    IGNORE_EINTR(::close(fd_));
}

bool MemlogSenderPipe::Connect() {
  return false;
}

bool MemlogSenderPipe::Send(const void* data, size_t sz) {
  return false;
}

}  // namespace profiling
