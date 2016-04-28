// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
#define CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace shell {
class Connection;
class Connector;
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
    virtual bool AcceptConnection(shell::Connection* connection) = 0;

    virtual ~Listener() {}
  };

  using Factory = base::Closure;
  // Sets the factory used to create the MojoShellConnection. This must be
  // called before the MojoShellConnection has been created.
  static void SetFactoryForTest(Factory* factory);

  // Will return null if no connection has been established (either because it
  // hasn't happened yet or the application was not spawned from the external
  // Mojo shell.
  static MojoShellConnection* Get();

  // Destroys the connection. Must be called on the thread the connection was
  // created on.
  static void Destroy();

  // Creates the appropriate MojoShellConnection from |request|. See
  // UsingExternalShell() for details of |is_external|.
  static void Create(shell::mojom::ShellClientRequest request,
                     bool is_external);

  virtual shell::Connector* GetConnector() = 0;

  virtual const shell::Identity& GetIdentity() const = 0;

  // Indicates whether the shell connection is to an external shell (true) or
  // a shell embedded in the browser process (false).
  virtual bool UsingExternalShell() const = 0;

  // Sets a closure that is called when the connection is lost. Note that
  // connection may already have been closed, in which case |closure| will be
  // run immediately before returning from this function.
  virtual void SetConnectionLostClosure(const base::Closure& closure) = 0;

  // [De]Register an impl of Listener that will be consulted when the wrapped
  // ShellConnection exposes services to inbound connections.
  // Registered listeners are owned by this MojoShellConnection. If a listener
  // is removed, then the ownership is transferred back to the caller.
  virtual void AddListener(std::unique_ptr<Listener> listener) = 0;
  virtual std::unique_ptr<Listener> RemoveListener(Listener* listener) = 0;

 protected:
  virtual ~MojoShellConnection();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
