// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_
#define MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_

#include <stdint.h>
#include <string>

#include "mojo/public/cpp/system/macros.h"
#include "mojo/shell/public/cpp/connection.h"

namespace mojo {

class Connector;

// An interface representing an instance "known to the Mojo Shell". The
// implementation receives lifecycle messages for the instance and gets the
// opportunity to handle inbound connections brokered by the Shell. Every client
// of ShellConnection must implement this interface, and instances of this
// interface must outlive the ShellConnection.
class ShellClient {
 public:
  ShellClient();
  virtual ~ShellClient();

  // Called once a bidirectional connection with the shell has been established.
  // |name| is the name used to start the application.
  // |id| is a unique identifier the shell uses to identify this specific
  // instance of the application.
  // |user_id| identifies the user this instance is run as.
  // Called exactly once before any other method.
  virtual void Initialize(Connector* connector,
                          const std::string& name,
                          uint32_t id,
                          uint32_t user_id = 0);

  // Called when a connection to this client is brokered by the shell. Override
  // to expose services to the remote application. Return true if the connection
  // should succeed. Return false if the connection should be rejected and the
  // underlying pipe closed. The default implementation returns false.
  virtual bool AcceptConnection(Connection* connection);

  // Called when ShellConnection's pipe to the Mojo Shell is closed.
  //
  // Returning true from this method will cause ...
  // The default implementation returns true.
  virtual bool ShellConnectionLost();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellClient);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_
