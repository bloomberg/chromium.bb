// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_CHILD_RUNNER_CONNECTION_H_
#define MOJO_RUNNER_CHILD_RUNNER_CONNECTION_H_

#include "base/macros.h"
#include "mojo/shell/public/interfaces/application.mojom.h"

namespace mojo {
namespace runner {

// Encapsulates a connection to a runner process. The connection object starts a
// background controller thread that is used to receive control messages from
// the runner. When this object is destroyed the thread is joined.
class RunnerConnection {
 public:
  virtual ~RunnerConnection();

  // Establish a connection to the runner, blocking the calling thread until
  // it is established. The Application request from the runner is returned via
  // |request|.
  // If a connection to the runner cannot be established, |request| will not be
  // modified and this function will return null.
  static RunnerConnection* ConnectToRunner(
      InterfaceRequest<Application>* request,
      ScopedMessagePipeHandle handle);

 protected:
  RunnerConnection();

 private:
  DISALLOW_COPY_AND_ASSIGN(RunnerConnection);
};

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_CHILD_RUNNER_CONNECTION_H_
