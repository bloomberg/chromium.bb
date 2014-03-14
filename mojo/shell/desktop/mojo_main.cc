// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"  // TODO(vtl): Remove.
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/child_process.h"
#include "mojo/shell/child_process_host.h"  // TODO(vtl): Remove.
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"
#include "mojo/shell/run.h"
#include "ui/gl/gl_surface.h"

namespace {

// TODO(vtl): Remove.
void DidStartTestChild(base::MessageLoop* message_loop, bool success) {
  VLOG(2) << "DidStartTestChild: success = " << success;
  message_loop->QuitWhenIdle();
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);
  mojo::shell::InitializeLogging();

  // TODO(vtl): Move this a proper test (and remove includes marked "remove").
  if (CommandLine::ForCurrentProcess()->HasSwitch("run-test-child")) {
    base::MessageLoop message_loop;
    mojo::shell::Context context;
    mojo::shell::ChildProcessHost child_process_host(
        &context, mojo::shell::ChildProcess::TYPE_TEST);
    child_process_host.Start(base::Bind(&DidStartTestChild, &message_loop));
    message_loop.Run();
    VLOG(2) << "Joined child: exit_code = " << child_process_host.Join();
    return 0;
  }

  // TODO(vtl): Unify parent and child process cases to the extent possible.
  if (scoped_ptr<mojo::shell::ChildProcess> child_process =
          mojo::shell::ChildProcess::Create(
              *CommandLine::ForCurrentProcess())) {
    base::MessageLoop message_loop;
    message_loop.PostTask(
        FROM_HERE,
        base::Bind(&mojo::shell::ChildProcess::Run,
                   base::Unretained(child_process.get())));

    message_loop.Run();
  } else {
    gfx::GLSurface::InitializeOneOff();

    base::MessageLoop message_loop;
    mojo::shell::Context context;
    message_loop.PostTask(FROM_HERE, base::Bind(mojo::shell::Run, &context));

    message_loop.Run();
  }

  return 0;
}
