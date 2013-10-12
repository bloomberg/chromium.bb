// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_EVENT_EMITTER_UDP_H_
#define LIBRARIES_NACL_IO_EVENT_EMITTER_UDP_H_

#include "nacl_io/event_emitter_stream.h"
#include "nacl_io/fifo_packet.h"

#include "sdk_util/macros.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class EventEmitterUDP;
typedef sdk_util::ScopedRef<EventEmitterUDP> ScopedEventEmitterUDP;

class EventEmitterUDP : public EventEmitterStream {
 public:
  EventEmitterUDP(size_t rsize, size_t wsize);

  // Takes or gives away ownership of the packet.
  Packet* ReadRXPacket_Locked();
  void WriteRXPacket_Locked(Packet* packet);

  Packet* ReadTXPacket_Locked();
  void WriteTXPacket_Locked(Packet* packet);

protected:
  virtual FIFOPacket* in_fifo() { return &in_fifo_; }
  virtual FIFOPacket* out_fifo() { return &out_fifo_; }

private:
  FIFOPacket in_fifo_;
  FIFOPacket out_fifo_;
  DISALLOW_COPY_AND_ASSIGN(EventEmitterUDP);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_EVENT_EMITTER_UDP_H_

