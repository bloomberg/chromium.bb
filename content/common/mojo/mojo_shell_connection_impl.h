// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
#define CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace content {

// Returns true for processes launched from an external mojo shell.
bool IsRunningInMojoShell();

class MojoShellConnectionImpl : public MojoShellConnection,
                                public mojo::ShellClient {
 public:
  // Creates an instance of this class and stuffs it in TLS on the calling
  // thread. Retrieve it using MojoShellConnection::Get().
  static void Create();

  // Like above but for initializing a connection to an embedded in-process
  // shell implementation. Binds to |request|.
  static void Create(mojo::shell::mojom::ShellClientRequest request);

  // Will return null if no connection has been established (either because it
  // hasn't happened yet or the application was not spawned from the external
  // Mojo shell).
  static MojoShellConnectionImpl* Get();

  // Binds the shell connection to a ShellClientFactory request pipe from the
  // command line. This must only be called once.
  void BindToRequestFromCommandLine();

 private:
  explicit MojoShellConnectionImpl(bool external);
  ~MojoShellConnectionImpl() override;

  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector,
                  const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;
  void ShellConnectionLost() override;

  // MojoShellConnection:
  mojo::Connector* GetConnector() override;
  bool UsingExternalShell() const override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;

  bool external_;
  scoped_ptr<mojo::ShellConnection> shell_connection_;
  std::vector<Listener*> listeners_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
