// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/event_emitter_tcp.h"

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "nacl_io/fifo_char.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

EventEmitterTCP::EventEmitterTCP(size_t rsize, size_t wsize)
    : in_fifo_(std::max<size_t>(65536, rsize)),
      out_fifo_(std::max<size_t>(65536, wsize)) {
}

uint32_t EventEmitterTCP::ReadIn_Locked(char* data, uint32_t len) {
  uint32_t count = in_fifo_.Read(data, len);

  UpdateStatus_Locked();
  return count;
}

uint32_t EventEmitterTCP::WriteIn_Locked(const char* data, uint32_t len) {
  uint32_t count = in_fifo_.Write(data, len);

  UpdateStatus_Locked();
  return count;
}

uint32_t EventEmitterTCP::ReadOut_Locked(char* data, uint32_t len) {
  uint32_t count = out_fifo_.Read(data, len);

  UpdateStatus_Locked();
  return count;
}

uint32_t EventEmitterTCP::WriteOut_Locked(const char* data, uint32_t len) {
  uint32_t count = out_fifo_.Write(data, len);

  UpdateStatus_Locked();
  return count;
}

void EventEmitterTCP::ConnectDone_Locked() {
  RaiseEvents_Locked(POLLOUT);
  UpdateStatus_Locked();
}

void EventEmitterTCP::SetAcceptedSocket_Locked(PP_Resource socket) {
  accepted_socket_ = socket;
  RaiseEvents_Locked(POLLIN);
}

PP_Resource EventEmitterTCP::GetAcceptedSocket_Locked() {
  int rtn = accepted_socket_;
  accepted_socket_ = 0;
  ClearEvents_Locked(POLLIN);
  return rtn;
}

}  // namespace nacl_io
