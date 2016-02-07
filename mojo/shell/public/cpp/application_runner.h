// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_APPLICATION_RUNNER_H_
#define MOJO_SHELL_PUBLIC_CPP_APPLICATION_RUNNER_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

class ShellClient;

// A utility for running a chromium based mojo Application. The typical use
// case is to use when writing your MojoMain:
//
//  MojoResult MojoMain(MojoHandle shell_handle) {
//    mojo::ApplicationRunner runner(new MyDelegate());
//    return runner.Run(shell_handle);
//  }
//
// ApplicationRunner takes care of chromium environment initialization and
// shutdown, and starting a MessageLoop from which your application can run and
// ultimately Quit().
class ApplicationRunner {
 public:
  // Takes ownership of |delegate|.
  explicit ApplicationRunner(ShellClient* client);
  ~ApplicationRunner();

  static void InitBaseCommandLine();

  void set_message_loop_type(base::MessageLoop::Type type);

  // Once the various parameters have been set above, use Run to initialize an
  // ApplicationImpl wired to the provided delegate, and run a MessageLoop until
  // the application exits.
  //
  // Iff |init_base| is true, the runner will perform some initialization of
  // base globals (e.g. CommandLine and AtExitManager) before starting the
  // application.
  MojoResult Run(MojoHandle shell_handle, bool init_base);

  // Calls Run above with |init_base| set to |true|.
  MojoResult Run(MojoHandle shell_handle);

 private:
  scoped_ptr<ShellClient> client_;

  // MessageLoop type. TYPE_CUSTOM is default (MessagePumpMojo will be used as
  // the underlying message pump).
  base::MessageLoop::Type message_loop_type_;
  // Whether Run() has been called.
  bool has_run_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationRunner);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_APPLICATION_RUNNER_H_
