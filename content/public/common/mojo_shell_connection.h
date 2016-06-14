// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
#define CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/common/mojo_application_info.h"
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
  using ShellClientRequestHandler =
      base::Callback<void(shell::mojom::ShellClientRequest)>;
  using Factory = base::Closure;

  virtual ~MojoShellConnection();

  // Sets the factory used to create the MojoShellConnection. This must be
  // called before the MojoShellConnection has been created.
  static void SetFactoryForTest(Factory* factory);

  // Will return null if no connection has been established (either because it
  // hasn't happened yet or the application was not spawned from the external
  // Mojo shell.
  static MojoShellConnection* Get();

  // Creates the appropriate MojoShellConnection from |request|. See
  // UsingExternalShell() for details of |is_external|. The instance created
  // by calling this method is accessible later from the same thread by calling
  // Get().
  static void Create(shell::mojom::ShellClientRequest request,
                     bool is_external);

  // Creates a MojoShellConnection from |request|. This instance is not thread-
  // global like the one created by Create().
  static std::unique_ptr<MojoShellConnection> CreateInstance(
      shell::mojom::ShellClientRequest request);

  // Destroys the connection. Must be called on the thread the connection was
  // created on.
  static void Destroy();

  // Returns the shell::Connector received via this connection's ShellClient
  // implementation. Use this to initiate connections as this object's Identity.
  virtual shell::Connector* GetConnector() = 0;

  // Returns this connection's identity with the shell. Connections initiated
  // via the shell::Connector returned by GetConnector() will use this.
  virtual const shell::Identity& GetIdentity() const = 0;

  // Indicates whether the shell connection is to an external shell (true) or
  // a shell embedded in the browser process (false).
  virtual bool UsingExternalShell() const = 0;

  // Sets a closure that is called when the connection is lost. Note that
  // connection may already have been closed, in which case |closure| will be
  // run immediately before returning from this function.
  virtual void SetConnectionLostClosure(const base::Closure& closure) = 0;

  // Allows the caller to expose interfaces to the caller using the identity of
  // this object's ShellClient. As distinct from AddEmbeddedService() and
  // AddShellClientRequestHandler() which specify unique identities for the
  // registered services.
  virtual void AddEmbeddedShellClient(
      std::unique_ptr<shell::ShellClient> shell_client) = 0;

  // Adds an embedded service to this connection's ShellClientFactory.
  // |info| provides details on how to construct new instances of the
  // service when an incoming connection is made to |name|.
  virtual void AddEmbeddedService(const std::string& name,
                                  const MojoApplicationInfo& info) = 0;

  // Adds a generic ShellClientRequestHandler for a given service name. This
  // will be used to satisfy any incoming calls to CreateShellClient() which
  // reference the given name.
  //
  // For in-process services, it is preferable to use |AddEmbeddedService()| as
  // defined above.
  virtual void AddShellClientRequestHandler(
      const std::string& name,
      const ShellClientRequestHandler& handler) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_SHELL_CONNECTION_H_
