// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/event_emitter_udp.h"

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "sdk_util/auto_lock.h"

namespace nacl_io {

EventEmitterUDP::EventEmitterUDP(size_t rsize, size_t wsize)
    : in_fifo_(rsize),
      out_fifo_(wsize) {
  UpdateStatus_Locked();
}

Packet* EventEmitterUDP::ReadRXPacket_Locked() {
  Packet* packet = in_fifo_.ReadPacket();

  UpdateStatus_Locked();
  return packet;
}

void EventEmitterUDP::WriteRXPacket_Locked(Packet* packet) {
  in_fifo_.WritePacket(packet);

  UpdateStatus_Locked();
}

Packet* EventEmitterUDP::ReadTXPacket_Locked() {
  Packet* packet = out_fifo_.ReadPacket();

  UpdateStatus_Locked();
  return packet;
}

void EventEmitterUDP::WriteTXPacket_Locked(Packet* packet) {
  out_fifo_.WritePacket(packet);

  UpdateStatus_Locked();
}

}  // namespace nacl_io


