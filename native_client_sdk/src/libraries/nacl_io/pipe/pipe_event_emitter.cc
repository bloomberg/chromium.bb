// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/pipe/pipe_event_emitter.h"

#include <algorithm>

namespace nacl_io {

PipeEventEmitter::PipeEventEmitter(size_t size)
    : fifo_(std::max<size_t>(1, size)) {
  UpdateStatus_Locked();
}

size_t PipeEventEmitter::Read_Locked(char* data, size_t len) {
  size_t out_len = fifo_.Read(data, len);

  UpdateStatus_Locked();
  return out_len;
}

size_t PipeEventEmitter::Write_Locked(const char* data, size_t len) {
  size_t out_len = fifo_.Write(data, len);

  UpdateStatus_Locked();
  return out_len;
}

}  // namespace nacl_io
