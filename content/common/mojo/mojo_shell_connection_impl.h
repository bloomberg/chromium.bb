// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
#define CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/interfaces/shell_client_factory.mojom.h"

namespace content {

class EmbeddedApplicationRunner;

// Returns true for processes launched from an external mojo shell.
bool IsRunningInMojoShell();

class MojoShellConnectionImpl
    : public MojoShellConnection,
      public shell::ShellClient,
      public shell::InterfaceFactory<shell::mojom::ShellClientFactory>,
      public shell::mojom::ShellClientFactory {

 public:
  // Creates the MojoShellConnection using MojoShellConnection::Factory. Returns
  // true if a factory was set and the connection was created, false otherwise.
  static bool CreateUsingFactory();

  // Creates an instance of this class and stuffs it in TLS on the calling
  // thread. Retrieve it using MojoShellConnection::Get(). Used only for
  // external shell.
  static void Create();

  static std::unique_ptr<MojoShellConnection> CreateInstance(
      shell::mojom::ShellClientRequest shell_client,
      bool is_external);

  // Will return null if no connection has been established (either because it
  // hasn't happened yet or the application was not spawned from the external
  // Mojo shell).
  static MojoShellConnectionImpl* Get();

  // Binds the shell connection to a ShellClientFactory request pipe from the
  // command line. This must only be called once.
  void BindToRequestFromCommandLine();

  // TODO(rockot): Remove this. http://crbug.com/594852.
  shell::ShellConnection* shell_connection() { return shell_connection_.get(); }

 private:
  friend class MojoShellConnection;

  explicit MojoShellConnectionImpl(bool external);
  ~MojoShellConnectionImpl() override;

  // MojoShellConnection:
  shell::Connector* GetConnector() override;
  const shell::Identity& GetIdentity() const override;
  bool UsingExternalShell() const override;
  void SetConnectionLostClosure(const base::Closure& closure) override;
  void AddEmbeddedShellClient(
      std::unique_ptr<shell::ShellClient> shell_client) override;
  void AddEmbeddedService(const std::string& name,
                          const MojoApplicationInfo& info) override;
  void AddShellClientRequestHandler(
      const std::string& name,
      const ShellClientRequestHandler& handler) override;

  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<shell::mojom::ShellClientFactory>:
  void Create(shell::Connection* connection,
              shell::mojom::ShellClientFactoryRequest request) override;

  // shell::mojom::ShellClientFactory:
  void CreateShellClient(shell::mojom::ShellClientRequest request,
                         const mojo::String& name) override;

  const bool external_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;
  mojo::BindingSet<shell::mojom::ShellClientFactory> factory_bindings_;
  std::vector<std::unique_ptr<shell::ShellClient>> embedded_shell_clients_;
  std::unordered_map<std::string, std::unique_ptr<EmbeddedApplicationRunner>>
      embedded_apps_;
  std::unordered_map<std::string, ShellClientRequestHandler> request_handlers_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
