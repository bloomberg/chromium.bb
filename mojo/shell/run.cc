// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/run.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/app_container.h"
#include "mojo/shell/switches.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

void Run(Context* context) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kApp)) {
    LOG(ERROR) << "No app path specified.";
    base::MessageLoop::current()->Quit();
    return;
  }

  AppContainer* container = new AppContainer(context);
  container->Load(GURL(command_line.GetSwitchValueASCII(switches::kApp)));
  // TODO(abarth): Currently we leak |container|.
}

}  // namespace shell
}  // namespace mojo
