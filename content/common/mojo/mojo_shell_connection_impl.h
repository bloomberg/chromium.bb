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
#include "mojo/shell/public/cpp/application_delegate.h"

namespace mojo {
namespace shell {
class RunnerConnection;
}
}

namespace content {

// Returns true for processes launched from an external mojo shell.
bool IsRunningInMojoShell();

class MojoShellConnectionImpl : public MojoShellConnection,
                                public mojo::ApplicationDelegate {
 public:
  // Creates an instance of this class and stuffs it in TLS on the calling
  // thread. Retrieve it using MojoShellConnection::Get().
  static void Create();

  // Will return null if no connection has been established (either because it
  // hasn't happened yet or the application was not spawned from the external
  // Mojo shell).
  static MojoShellConnectionImpl* Get();

  // Blocks the calling thread until calling GetApplication() will return an
  // Initialized() application with a bound ShellPtr. This call is a no-op
  // if the connection has already been initialized.
  void BindToCommandLinePlatformChannel();

  // Same as BindToCommandLinePlatformChannel(), but receives a |handle| instead
  // of looking for one on the command line.
  void BindToMessagePipe(mojo::ScopedMessagePipeHandle handle);

 private:
  MojoShellConnectionImpl();
  ~MojoShellConnectionImpl() override;

  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* application) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // MojoShellConnection:
  mojo::ApplicationImpl* GetApplication() override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;

  // Blocks the calling thread until a connection to the spawning shell is
  // established, an Application request from it is bound, and the Initialize()
  // method on that application is called.
  void WaitForShell(mojo::ScopedMessagePipeHandle handle);

  bool initialized_;
  scoped_ptr<mojo::shell::RunnerConnection> runner_connection_;
  scoped_ptr<mojo::ApplicationImpl> application_impl_;
  std::vector<Listener*> listeners_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SHELL_CONNECTION_IMPL_H_
