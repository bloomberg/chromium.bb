// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/child_process.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/shell/app_child_process.h"
#include "mojo/shell/switches.h"

namespace mojo {
namespace shell {

ChildProcess::~ChildProcess() {
}

// static
scoped_ptr<ChildProcess> ChildProcess::Create(
    const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switches::kChildProcess))
    return scoped_ptr<ChildProcess>();

  scoped_ptr<ChildProcess> rv(new AppChildProcess());
  if (!rv)
    return nullptr;

  rv->platform_channel_ =
      embedder::PlatformChannelPair::PassClientHandleFromParentProcess(
          command_line);
  CHECK(rv->platform_channel_.is_valid());
  return rv;
}

ChildProcess::ChildProcess() {
}

}  // namespace shell
}  // namespace mojo
