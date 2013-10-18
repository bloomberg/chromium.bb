// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/app_container.h"
#include "mojo/shell/storage.h"
#include "mojo/shell/switches.h"
#include "mojo/shell/task_runners.h"
#include "mojo/system/core_impl.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);

  mojo::system::CoreImpl::Init();

  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);

  // TODO(abarth): Group these objects into a "context" object.
  mojo::shell::TaskRunners task_runners(message_loop.message_loop_proxy());
  mojo::shell::Storage storage;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kApp)) {
    LOG(ERROR) << "No app path specified.";
    return 0;
  }

  scoped_ptr<mojo::shell::AppContainer> container(
      new mojo::shell::AppContainer);
  container->LaunchApp(command_line.GetSwitchValuePath(switches::kApp));
  message_loop.Run();
  return 0;
}
