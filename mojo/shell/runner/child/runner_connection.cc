// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/runner/child/runner_connection.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"
#include "mojo/shell/runner/common/switches.h"

namespace mojo {
namespace shell {
namespace {

class RunnerConnectionImpl : public RunnerConnection,
                             public mojom::ShellClientFactory {
 public:
  RunnerConnectionImpl(mojo::ShellConnection* shell_connetion,
                       mojom::ShellClientFactoryRequest client_factory_request,
                       bool exit_on_error)
      : shell_connection_(shell_connetion),
        binding_(this, std::move(client_factory_request)),
        exit_on_error_(exit_on_error) {
    binding_.set_connection_error_handler([this] { OnConnectionError(); });
  }

  ~RunnerConnectionImpl() override {}

 private:
  // mojom::ShellClientFactory:
  void CreateShellClient(mojom::ShellClientRequest client_request,
                         const String& name) override {
    shell_connection_->BindToRequest(std::move(client_request));
  }

  void OnConnectionError() {
    // A connection error means the connection to the shell is lost. This is not
    // recoverable.
    DLOG(ERROR) << "Connection error to the shell.";
    if (exit_on_error_)
      _exit(1);
  }

  mojo::ShellConnection* const shell_connection_;
  Binding<mojom::ShellClientFactory> binding_;
  const bool exit_on_error_;

  DISALLOW_COPY_AND_ASSIGN(RunnerConnectionImpl);
};

}  // namespace

RunnerConnection::~RunnerConnection() {}

// static
scoped_ptr<RunnerConnection> RunnerConnection::Create(
    mojo::ShellConnection* shell_connection,
    bool exit_on_error) {
  std::string primordial_pipe_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPrimordialPipeToken);
  mojom::ShellClientFactoryRequest client_factory_request;
  client_factory_request.Bind(
      edk::CreateChildMessagePipe(primordial_pipe_token));

  return make_scoped_ptr(new RunnerConnectionImpl(
      shell_connection, std::move(client_factory_request), exit_on_error));
}

RunnerConnection::RunnerConnection() {}

}  // namespace shell
}  // namespace mojo
