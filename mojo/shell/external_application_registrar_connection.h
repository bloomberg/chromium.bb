// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_EXTERNAL_APPLICATION_REGISTRAR_CONNECTION_H_
#define MOJO_SHELL_EXTERNAL_APPLICATION_REGISTRAR_CONNECTION_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/edk/embedder/channel_init.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/interfaces/application/application.mojom.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/shell/external_application_registrar.mojom.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

// Externally-running applications can use this class to discover and register
// with a running mojo_shell instance.
// MUST be run on an IO thread
class ExternalApplicationRegistrarConnection : public ErrorHandler {
 public:
  // Configures client_socket_ to point at socket_path.
  explicit ExternalApplicationRegistrarConnection(
      const base::FilePath& socket_path);
  ~ExternalApplicationRegistrarConnection() override;

  // Implementation of ErrorHandler
  void OnConnectionError() override;

  // Connects client_socket_ and binds it to registrar_.
  // Status code is passed to callback upon success or failure.
  // May return either synchronously or asynchronously, depending on the
  // status of the underlying socket.
  void Connect(const net::CompletionCallback& callback);

  // Registers this app with the shell at the provided URL.
  // shell is not ready for use until register_complete_callback fires.
  // TODO(cmasone): Once the pipe for shell can be placed in a FIFO relationship
  // with the one underlying registrar_, the callback becomes unneeded.
  void Register(const GURL& app_url,
                ShellPtr* shell,
                base::Closure register_complete_callback);

 private:
  // Handles the result of Connect(). If it was successful, promotes the socket
  // to a MessagePipe and binds it to registrar_.
  // Hands rv to callback regardless.
  void OnConnect(net::CompletionCallback callback, int rv);

  scoped_ptr<net::UnixDomainClientSocket> client_socket_;
  mojo::embedder::ChannelInit channel_init_;
  ExternalApplicationRegistrarPtr registrar_;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_EXTERNAL_APPLICATION_REGISTRAR_CONNECTION_H_
