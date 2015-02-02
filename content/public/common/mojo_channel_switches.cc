// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/mojo_channel_switches.h"

#include "base/command_line.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "mojo/common/common_type_converters.h"

namespace switches {

// Replaces renderer-browser IPC channel with ChnanelMojo.
// TODO(morrita): Now ChannelMojo for the renderer is on by default.
// Remove this once the change sticks.
const char kEnableRendererMojoChannel[] =
    "enable-renderer-mojo-channel";

// Disale ChannelMojo usage regardless of the platform or the process type.
const char kDisableMojoChannel[] = "disable-mojo-channel";

}  // namespace switches

namespace content {

bool ShouldUseMojoChannel() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableMojoChannel))
    return false;
  if (command_line.HasSwitch(switches::kEnableRendererMojoChannel))
    return true;
  return IPC::ChannelMojo::ShouldBeUsed();
}

}  // namespace content
