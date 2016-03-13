// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_RUNNER_CHILD_RUNNER_CONNECTION_H_
#define MOJO_SHELL_RUNNER_CHILD_RUNNER_CONNECTION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"

namespace mojo {
namespace shell {

// Encapsulates a connection to a runner process.
class RunnerConnection {
 public:
  virtual ~RunnerConnection();

  // Establishes a new runner connection using a ShellClientFactory request pipe
  // from the command line.
  //
  // |shell_connection| is a ShellConnection which will be used to service
  // incoming connection requests from other clients. This may be null, in which
  // case no incoming connection requests will be serviced. This is not owned
  // and must out-live the RunnerConnection.
  //
  // If |exit_on_error| is true, the calling process will be terminated in the
  // event of an error on the ShellClientFactory pipe.
  //
  // The RunnerConnection returned by this call bounds the lifetime of the
  // connection. If connection fails (which can happen if there is no
  // ShellClientFactory request pipe on the command line) this returns null.
  //
  // TODO(rockot): Remove |exit_on_error|. We shouldn't be killing processes
  // abruptly on pipe errors, but some tests currently depend on this behavior.
  static scoped_ptr<RunnerConnection> Create(
      mojo::ShellConnection* shell_connection,
      bool exit_on_error = true);

 protected:
  RunnerConnection();

 private:
  DISALLOW_COPY_AND_ASSIGN(RunnerConnection);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_RUNNER_CHILD_RUNNER_CONNECTION_H_
