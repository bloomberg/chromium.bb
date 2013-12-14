// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "nacl_io/event_emitter_pipe.h"

namespace nacl_io {

EventEmitterPipe::EventEmitterPipe(size_t size)
    : fifo_(std::max<size_t>(1, size)) {
  UpdateStatus_Locked();
}

size_t EventEmitterPipe::Read_Locked(char* data, size_t len) {
  size_t out_len = fifo_.Read(data, len);

  UpdateStatus_Locked();
  return out_len;
}

size_t EventEmitterPipe::Write_Locked(const char* data, size_t len) {
  size_t out_len = fifo_.Write(data, len);

  UpdateStatus_Locked();
  return out_len;
}

}  // namespace nacl_io

