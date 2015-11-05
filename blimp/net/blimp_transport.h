// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_TRANSPORT_H_
#define BLIMP_NET_BLIMP_TRANSPORT_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"

namespace blimp {

class BlimpConnection;

// An interface which encapsulates the transport-specific code for
// establishing network connections between the client and engine.
// Subclasses of BlimpTransport are responsible for defining their own
// methods for receiving connection arguments.
class BlimpTransport {
 public:
  virtual ~BlimpTransport() {}

  // Initiate or listen for a connection.
  //
  // Returns net::OK if a connection is established synchronously.
  // Returns an error code if a synchronous error occurred.
  // Returns net::ERR_IO_PENDING if the connection is being established
  // asynchronously. |callback| is later invoked with the connection outcome.
  //
  // If the connection is successful, the BlimpConnection can be taken by
  // calling TakeConnectedSocket().
  virtual int Connect(const net::CompletionCallback& callback) = 0;

  // Returns the connection object after a successful Connect().
  virtual scoped_ptr<BlimpConnection> TakeConnection() = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_TRANSPORT_H_
