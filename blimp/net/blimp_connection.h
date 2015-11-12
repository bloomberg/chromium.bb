// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_CONNECTION_H_
#define BLIMP_NET_BLIMP_CONNECTION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BlimpMessageProcessor;
class PacketReader;
class PacketWriter;

// Encapsulates the state and logic used to exchange BlimpMessages over
// a network connection.
class BLIMP_NET_EXPORT BlimpConnection {
 public:
  class DisconnectObserver {
    // Called when the network connection for |this| is disconnected.
    virtual void OnDisconnected() = 0;
  };

  BlimpConnection(scoped_ptr<PacketReader> reader,
                  scoped_ptr<PacketWriter> writer);

  virtual ~BlimpConnection();

  // Lets |observer| know when the network connection is terminated.
  void AddDisconnectObserver(DisconnectObserver* observer);

  // Sets the processor which will take incoming messages for this connection.
  // Can be set multiple times, but previously set processors are discarded.
  void set_incoming_message_processor(
      scoped_ptr<BlimpMessageProcessor> processor);

  // Gets a processor for BrowserSession->BlimpConnection message routing.
  scoped_ptr<BlimpMessageProcessor> take_outgoing_message_processor() const;

 private:
  scoped_ptr<PacketReader> reader_;
  scoped_ptr<PacketWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(BlimpConnection);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_CONNECTION_H_
