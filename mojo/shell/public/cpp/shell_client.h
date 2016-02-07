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

// An abstract class that the application may subclass to control various
// behaviors of ApplicationImpl.
class ShellClient {
 public:
  ShellClient();
  virtual ~ShellClient();

  // Called once a bidirectional connection with the shell has been established.
  // |url| is the URL used to start the application. |id| is a unique identifier
  // the shell uses to identify this specific instance of the application.
  // Called exactly once before any other method.
  virtual void Initialize(Shell* shell, const std::string& url, uint32_t id);

  // Override this method to configure what services a connection supports when
  // being connected to from an app.
  // Return false to reject the connection entirely. The default implementation
  // returns false.
  virtual bool AcceptConnection(Connection* connection);

  // Called when the shell connection has a connection error.
  //
  // Return true to shutdown the application. Return false to skip shutting
  // down the connection, but user is then required to call
  // ApplicationImpl::QuitNow() when done. Default implementation returns true.
  virtual bool ShellConnectionLost();

  // Called before ApplicationImpl::Terminate(). After returning from this call
  // the delegate can no longer rely on the main run loop still running.
  virtual void Quit();

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(ShellClient);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_
