// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BROWSER_CONNECTION_HANDLER_H_
#define BLIMP_NET_BROWSER_CONNECTION_HANDLER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/connection_handler.h"

namespace blimp {

class BlimpConnection;
class BlimpMessageCheckpointer;
class BlimpMessageDemultiplexer;
class BlimpMessageMultiplexer;
class BlimpMessageOutputBuffer;
class BlimpMessageProcessor;

// Routes incoming messages to their respective features, and buffers and sends
// messages out via underlying BlimpConnection.
// A BrowserConnectionHandler is created on browser startup, and persists for
// the lifetime of the application.
class BLIMP_NET_EXPORT BrowserConnectionHandler
    : public ConnectionHandler,
      public ConnectionErrorObserver {
 public:
  BrowserConnectionHandler();
  ~BrowserConnectionHandler() override;

  // Registers a message processor which will receive all messages of the |type|
  // specified. Only one handler may be added per type.
  // That caller must ensure |incoming_processor| remains valid while
  // this object is in-use.
  //
  // Returns a BlimpMessageProcessor object for sending messages of type |type|.
  scoped_ptr<BlimpMessageProcessor> RegisterFeature(
      BlimpMessage::Type type,
      BlimpMessageProcessor* incoming_processor);

  // ConnectionHandler implementation.
  void HandleConnection(scoped_ptr<BlimpConnection> connection) override;

  // ConnectionErrorObserver implementation.
  void OnConnectionError(int error) override;

 private:
  void DropCurrentConnection();

  // Routes incoming messages to the relevant feature-specific handlers.
  scoped_ptr<BlimpMessageDemultiplexer> demultiplexer_;

  // Provides buffering of outgoing messages, for use in session-recovery.
  scoped_ptr<BlimpMessageOutputBuffer> output_buffer_;

  // Routes outgoing messages from feature-specific handlers to a single
  // message stream.
  scoped_ptr<BlimpMessageMultiplexer> multiplexer_;

  // Dispatches checkpoint/ACK messages to the outgoing processor, as the
  // incoming processor completes processing them.
  scoped_ptr<BlimpMessageCheckpointer> checkpointer_;

  // Holds network resources while there is a Client connected.
  scoped_ptr<BlimpConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserConnectionHandler);
};

}  // namespace blimp

#endif  // BLIMP_NET_BROWSER_CONNECTION_HANDLER_H_
