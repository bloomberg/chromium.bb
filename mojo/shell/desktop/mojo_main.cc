// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/child_process.h"
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"
#include "mojo/shell/switches.h"
#include "ui/gfx/switches.h"

#if defined(COMPONENT_BUILD)
#include "ui/gl/gl_surface.h"
#endif

namespace {

void RunApps(mojo::shell::Context* context) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line.GetArgs();
  for (base::CommandLine::StringVector::const_iterator it = args.begin();
       it != args.end();
       ++it) {
    context->Run(GURL(*it));
  }
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
#if defined(OS_LINUX)
  // We use gfx::RenderText from multiple threads concurrently and the pango
  // backend (currently the default on linux) is not close to threadsafe. Force
  // use of the harfbuzz backend for now.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableHarfBuzzRenderText);
#endif
  mojo::shell::InitializeLogging();

  // TODO(vtl): Unify parent and child process cases to the extent possible.
  if (scoped_ptr<mojo::shell::ChildProcess> child_process =
          mojo::shell::ChildProcess::Create(
              *base::CommandLine::ForCurrentProcess())) {
    child_process->Main();
  } else {
#if defined(COMPONENT_BUILD)
    gfx::GLSurface::InitializeOneOff();
#endif
    // We want the shell::Context to outlive the MessageLoop so that pipes are
    // all gracefully closed / error-out before we try to shut the Context down.
    mojo::shell::Context shell_context;
    {
      base::MessageLoop message_loop;
      shell_context.Init();

      const base::CommandLine& command_line =
          *base::CommandLine::ForCurrentProcess();
      if (command_line.HasSwitch(switches::kOrigin)) {
        shell_context.mojo_url_resolver()->SetBaseURL(
            GURL(command_line.GetSwitchValueASCII(switches::kOrigin)));
      }

      message_loop.PostTask(FROM_HERE, base::Bind(RunApps, &shell_context));
      message_loop.Run();
    }
  }
  return 0;
}
