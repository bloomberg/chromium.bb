// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_TRANSPORT_H_
#define BLIMP_NET_BLIMP_TRANSPORT_H_

#include <memory>
#include <string>

#include "net/base/completion_callback.h"

namespace blimp {

class BlimpConnection;
class MessagePort;

// An interface which encapsulates the transport-specific code for
// establishing network connections between the client and engine.
// Subclasses of BlimpTransport are responsible for defining their own
// methods for receiving connection arguments.
class BlimpTransport {
 public:
  virtual ~BlimpTransport() {}

  // Initiate or listen for a connection.
  //
  // |callback| is passed net::OK if a connection was successfully
  // established. The connection's MessagePort can then be taken by calling
  // TakeMessagePort().
  // All other values indicate a connection error.
  virtual void Connect(const net::CompletionCallback& callback) = 0;

  // Returns the MessagePort of a successfully established connection.
  virtual std::unique_ptr<MessagePort> TakeMessagePort() = 0;

  // Gets the transport name, e.g. "TCP", "SSL", "mock", etc.
  virtual const char* GetName() const = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_TRANSPORT_H_
