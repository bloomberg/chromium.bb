// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOJO_RUNNER_STATE_H_
#define CHROME_BROWSER_MOJO_RUNNER_STATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace mojo {
class ApplicationImpl;
namespace runner {
class RunnerConnection;
}
}

class ChromeApplicationDelegate;

// Encapsulates a connection to a spawning mojo_runner/shell.
// TODO(beng): figure out if/how this should be exposed to other layers
//             of code in Chrome.
class MojoRunnerState {
 public:
  MojoRunnerState();
  ~MojoRunnerState();

  // Blocks the calling thread until a connection to the spawning mojo_runner
  // is established, an Application request from it is bound, and the
  // Initialize() method on that application is called.
  void WaitForConnection();

 private:
  scoped_ptr<mojo::runner::RunnerConnection> runner_connection_;
  scoped_ptr<mojo::ApplicationImpl> application_impl_;
  scoped_ptr<ChromeApplicationDelegate> application_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MojoRunnerState);
};

#endif  // CHROME_BROWSER_MOJO_RUNNER_STATE_H_
