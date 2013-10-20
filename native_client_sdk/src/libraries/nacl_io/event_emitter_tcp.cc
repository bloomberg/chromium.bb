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
    : in_fifo_(rsize),
      out_fifo_(wsize),
      error_(false),
      listening_(false),
      accepted_socket_(0) {
}

uint32_t EventEmitterTCP::ReadIn_Locked(char* data, uint32_t len) {
  uint32_t count = in_fifo_.Read(data, len);
  UpdateStatus_Locked();
  return count;
}

void EventEmitterTCP::UpdateStatus_Locked() {
  if (error_) {
    RaiseEvents_Locked(POLLIN | POLLOUT);
    return;
  }

  if (listening_) {
    if (accepted_socket_)
      RaiseEvents_Locked(POLLIN);
    return;
  }

  EventEmitterStream::UpdateStatus_Locked();
}

void EventEmitterTCP::SetListening_Locked() {
  listening_ = true;
  UpdateStatus_Locked();
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

bool EventEmitterTCP::GetError_Locked() {
  return error_;
}

void EventEmitterTCP::SetError_Locked() {
  error_ = true;
  UpdateStatus_Locked();
}

void EventEmitterTCP::SetAcceptedSocket_Locked(PP_Resource socket) {
  accepted_socket_ = socket;
  UpdateStatus_Locked();
}

PP_Resource EventEmitterTCP::GetAcceptedSocket_Locked() {
  int rtn = accepted_socket_;
  accepted_socket_ = 0;
  UpdateStatus_Locked();
  return rtn;
}

}  // namespace nacl_io
