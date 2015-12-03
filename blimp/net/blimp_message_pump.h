// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_MESSAGE_PUMP_H_
#define BLIMP_NET_BLIMP_MESSAGE_PUMP_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/blimp_net_export.h"
#include "net/base/completion_callback.h"

namespace net {
class GrowableIOBuffer;
}

namespace blimp {

class BlimpMessageProcessor;
class ConnectionErrorObserver;
class PacketReader;

// Reads and deserializes incoming packets from |reader_|, and forwards parsed
// BlimpMessages to |processor_|. When |processor_| is ready to take the next
// message, the BlimpMessagePump reads the next packet.
class BLIMP_NET_EXPORT BlimpMessagePump {
 public:
  // Caller ensures that |reader| outlives this object.
  explicit BlimpMessagePump(PacketReader* reader);

  ~BlimpMessagePump();

  // Sets the processor which will take BlimpMessages. Can only be set once.
  // Caller retains the ownership of |processor|.
  void SetMessageProcessor(BlimpMessageProcessor* processor);

  void set_error_observer(ConnectionErrorObserver* observer) {
    error_observer_ = observer;
  }

 private:
  // Read next packet from |reader_|.
  void ReadNextPacket();

  // Callback when next packet is ready in |buffer_|.
  void OnReadPacketComplete(int result);

  // Callback when |processor_| finishes processing a BlimpMessage.
  void OnProcessMessageComplete(int result);

  PacketReader* reader_;
  ConnectionErrorObserver* error_observer_;
  BlimpMessageProcessor* processor_;
  scoped_refptr<net::GrowableIOBuffer> buffer_;

  // Cancelled in the event that the connection is destroyed (along with
  // |this|) while a inflight callback is held by |processor_|.
  net::CancelableCompletionCallback process_msg_callback_;

  // Cancelled to guard against |this| being called back from a completed read
  // operation.
  net::CancelableCompletionCallback read_callback_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessagePump);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_MESSAGE_PUMP_H_
