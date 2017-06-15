// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_globals.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"

namespace profiling {

ProfilingGlobals::ProfilingGlobals()
    : io_thread_("IO thread") {
#if defined(OS_WIN)
  io_thread_.init_com_with_mta(true);
#endif
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);
}

ProfilingGlobals::~ProfilingGlobals() {}

// static
ProfilingGlobals* ProfilingGlobals::Get() {
  static ProfilingGlobals singleton;
  return &singleton;
}

base::TaskRunner* ProfilingGlobals::GetIORunner() {
  return io_thread_.task_runner().get();
}

scoped_refptr<base::SingleThreadTaskRunner> ProfilingGlobals::GetMainThread()
    const {
  CHECK(base::MessageLoop::current() == main_message_loop_);
  return main_message_loop_->task_runner();
}

void ProfilingGlobals::RunMainMessageLoop() {
  // TODO(brettw) if we never add anything interesting on the main thread here,
  // we can change this so the main thread *is* the I/O thread. This will save
  // some resources.
  base::MessageLoopForUI message_loop;
  DCHECK(!main_message_loop_);
  main_message_loop_ = &message_loop;

  base::RunLoop run_loop;
  run_loop.Run();

  main_message_loop_ = nullptr;
}

void ProfilingGlobals::QuitWhenIdle() {
  main_message_loop_->QuitWhenIdle();
}

}  // namespace profiling
