// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/event_emitter_stream.h"

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "nacl_io/fifo_interface.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

EventEmitterStream::EventEmitterStream() : stream_(NULL) {}

void EventEmitterStream::AttachStream(MountNodeStream* stream) {
  AUTO_LOCK(GetLock());
  stream_ = stream;
}

void EventEmitterStream::DetachStream() {
  AUTO_LOCK(GetLock());

  RaiseEvents_Locked(POLLHUP);
  stream_ = NULL;
}

void EventEmitterStream::UpdateStatus_Locked() {
  uint32_t status = 0;
  if (!in_fifo()->IsEmpty())
    status |= POLLIN;

  if (!out_fifo()->IsFull())
    status |= POLLOUT;

  ClearEvents_Locked(~status);
  RaiseEvents_Locked(status);
}

uint32_t EventEmitterStream::BytesInOutputFIFO() {
  return out_fifo()->ReadAvailable();
}

uint32_t EventEmitterStream::SpaceInInputFIFO() {
  return in_fifo()->WriteAvailable();
}

}  // namespace nacl_io
