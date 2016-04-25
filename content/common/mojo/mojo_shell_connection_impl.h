// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
#define CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"

namespace content {

// Returns true for processes launched from an external mojo shell.
bool IsRunningInMojoShell();

class MojoShellConnectionImpl : public MojoShellConnection,
                                public shell::ShellClient {
 public:
  // Creates the MojoShellConnection using MojoShellConnection::Factory. Returns
  // true if a factory was set and the connection was created, false otherwise.
  static bool CreateUsingFactory();

  // Creates an instance of this class and stuffs it in TLS on the calling
  // thread. Retrieve it using MojoShellConnection::Get().
  static void Create();

  // Will return null if no connection has been established (either because it
  // hasn't happened yet or the application was not spawned from the external
  // Mojo shell).
  static MojoShellConnectionImpl* Get();

  // Binds the shell connection to a ShellClientFactory request pipe from the
  // command line. This must only be called once.
  void BindToRequestFromCommandLine();

 private:
  friend class MojoShellConnection;

  explicit MojoShellConnectionImpl(bool external);
  ~MojoShellConnectionImpl() override;

  void WaitForShellIfNecessary();

  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // MojoShellConnection:
  shell::Connector* GetConnector() override;
  const shell::Identity& GetIdentity() const override;
  bool UsingExternalShell() const override;
  void SetConnectionLostClosure(const base::Closure& closure) override;
  void AddListener(std::unique_ptr<Listener> listener) override;
  std::unique_ptr<Listener> RemoveListener(Listener* listener) override;

  const bool external_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;
  std::vector<Listener*> listeners_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
