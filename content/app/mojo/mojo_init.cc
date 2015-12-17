// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

#if defined(MOJO_SHELL_CLIENT)
#include "content/common/mojo/mojo_shell_connection_impl.h"
#endif

namespace content {

namespace {

class MojoInitializer {
 public:
  MojoInitializer() {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);
    if (0 && process_type.empty() && !command_line.HasSwitch("use-old-edk")) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          "use-new-edk");
    }

    if (command_line.HasSwitch("use-new-edk")) {
      bool initialize_as_parent = process_type.empty();
#if defined(MOJO_SHELL_CLIENT)
      if (IsRunningInMojoShell())
        initialize_as_parent = false;
#endif
      if (initialize_as_parent) {
        mojo::embedder::PreInitializeParentProcess();
      } else {
        mojo::embedder::PreInitializeChildProcess();
      }
    }

    mojo::embedder::SetMaxMessageSize(IPC::Channel::kMaximumMessageSize);
    mojo::embedder::Init();
  }
};

base::LazyInstance<MojoInitializer>::Leaky mojo_initializer;

}  //  namespace

void InitializeMojo() {
  mojo_initializer.Get();
}

}  // namespace content
