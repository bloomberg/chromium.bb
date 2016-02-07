// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
#define CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_

#include "content/common/content_export.h"

namespace mojo {
class Connection;
class Shell;
}

namespace content {

// Encapsulates a connection to a spawning external Mojo shell.
// Access an instance by calling Get(), on the thread the Shell connection is
// bound. Clients can implement Listener, which allows them to register services
// to expose to inbound connections. Clients should call this any time after
// the main message loop is created but not yet run (e.g. in the browser process
// this object is created in PreMainMessageLoopRun(), so the BrowserMainParts
// impl can access this in its implementation of that same method.
class CONTENT_EXPORT MojoShellConnection {
 public:
  // Override to add additional services to inbound connections.
  // TODO(beng): This should just be ShellClient.
  class Listener {
   public:
    virtual bool AcceptConnection(mojo::Connection* connection) = 0;

    virtual ~Listener() {}
  };

  // Will return null if no connection has been established (either because it
  // hasn't happened yet or the application was not spawned from the external
  // Mojo shell.
  static MojoShellConnection* Get();

  // Destroys the connection. Must be called on the thread the connection was
  // created on.
  static void Destroy();

  // Returns an Initialized() Shell.
  virtual mojo::Shell* GetShell() = 0;

  // [De]Register an impl of Listener that will be consulted when the wrapped
  // ApplicationImpl exposes services to inbound connections.
  // Registered listeners are owned by this MojoShellConnection.
  virtual void AddListener(Listener* listener) = 0;
  virtual void RemoveListener(Listener* listener) = 0;

 protected:
  virtual ~MojoShellConnection();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
