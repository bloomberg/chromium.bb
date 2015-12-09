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
class BlimpMessagePump;
class ConnectionErrorObserver;
class PacketReader;
class PacketWriter;

// Encapsulates the state and logic used to exchange BlimpMessages over
// a network connection.
class BLIMP_NET_EXPORT BlimpConnection {
 public:
  BlimpConnection(scoped_ptr<PacketReader> reader,
                  scoped_ptr<PacketWriter> writer);

  virtual ~BlimpConnection();

  // Lets |observer| know when the network connection encounters an error.
  virtual void SetConnectionErrorObserver(ConnectionErrorObserver* observer);

  // Sets the processor which will take incoming messages for this connection.
  // Can be set multiple times, but previously set processors are discarded.
  // Caller retains the ownership of |processor|.
  virtual void SetIncomingMessageProcessor(BlimpMessageProcessor* processor);

  // Gets a processor for BrowserSession->BlimpConnection message routing.
  virtual BlimpMessageProcessor* GetOutgoingMessageProcessor() const;

 protected:
  BlimpConnection();

 private:
  scoped_ptr<PacketReader> reader_;
  scoped_ptr<BlimpMessagePump> message_pump_;
  scoped_ptr<PacketWriter> writer_;
  scoped_ptr<BlimpMessageProcessor> outgoing_msg_processor_;

  DISALLOW_COPY_AND_ASSIGN(BlimpConnection);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_CONNECTION_H_
