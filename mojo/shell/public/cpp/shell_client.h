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

class Shell;

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
  // |url| is the URL used to start the application. |id| is a unique identifier
  // the shell uses to identify this specific instance of the application.
  // Called exactly once before any other method.
  virtual void Initialize(Shell* shell, const std::string& url, uint32_t id);

  // Called when a connection to this client is brokered by the shell. Override
  // to expose services to the remote application. Return true if the connection
  // should succeed. Return false if the connection should be rejected and the
  // underlying pipe closed. The default implementation returns false.
  virtual bool AcceptConnection(Connection* connection);

  // Called when ShellConnection's pipe to the Mojo Shell is closed.
  //
  // Returning true from this method will cause the ShellConnection instance to
  // call this instance back via Quit(), and then run the termination closure
  // passed to it (which may do cleanup like, for example, quitting a run loop).
  // Returning false from this method will not do any of this. The client is
  // then responsible for calling Shell::QuitNow() when it is ready to close.
  // The client may do this if it wishes to continue servicing connections other
  // than the Shell.
  // The default implementation returns true.
  virtual bool ShellConnectionLost();

  // Called before ShellConnection::QuitNow(). After returning from this call
  // the delegate can no longer rely on the main run loop still running.
  virtual void Quit();

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ShellClient);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_
