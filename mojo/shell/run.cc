// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/loader/loader.h"
#include "mojo/shell/app_container.h"
#include "mojo/shell/storage.h"
#include "mojo/shell/switches.h"
#include "mojo/shell/task_runners.h"
#include "mojo/system/core_impl.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

void Run() {
  system::CoreImpl::Init();

#if defined(OS_ANDROID)
  return;  // TODO(abarth): Run more of RunShell on Android.
#endif

  // TODO(abarth): Group these objects into a "context" object.
  TaskRunners task_runners(base::MessageLoop::current()->message_loop_proxy());
  Storage storage;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kApp)) {
    LOG(ERROR) << "No app path specified.";
    return;
  }

  loader::Loader loader(task_runners.io_runner(),
                        task_runners.file_runner(),
                        storage.profile_path());

  scoped_ptr<AppContainer> container(new AppContainer);

  scoped_ptr<loader::Job> job = loader.Load(
    GURL(command_line.GetSwitchValueASCII(switches::kApp)),
    container.get());
}

}  // namespace shell
}  // namespace mojo
