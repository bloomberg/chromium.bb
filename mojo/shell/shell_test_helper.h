// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_HELPER_
#define MOJO_SHELL_SHELL_TEST_HELPER_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"

namespace base {
class MessageLoopProxy;
class RunLoop;
}

namespace mojo {
namespace shell {

// ShellTestHelper is useful for tests to establish a connection to the Shell.
// ShellTestHelper does this by spawning a thread and connecting. Invoke Init()
// to do this. Once done, shell() returns the handle to the Shell.
class ShellTestHelper {
 public:
  struct State;

  ShellTestHelper();
  ~ShellTestHelper();

  void Init();

  // Returns a handle to the Shell. ShellTestHelper owns the shell.
  Shell* shell() { return shell_.get(); }

 private:
  class TestShellClient;

  // Invoked once connection has been established.
  void OnShellStarted();

  Environment environment_;

  base::Thread shell_thread_;

  // If non-null we're in Init() and waiting for connection.
  scoped_ptr<base::RunLoop> run_loop_;

  // See comment in declaration for details.
  State* state_;

  // Client interface for the shell.
  scoped_ptr<TestShellClient> shell_client_;

  ShellPtr shell_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestHelper);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_TEST_HELPER_
