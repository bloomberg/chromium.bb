// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_globals.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"

namespace profiling {

ProfilingGlobals* ProfilingGlobals::singleton_ = nullptr;

ProfilingGlobals::ProfilingGlobals() {
  DCHECK(!singleton_);
  singleton_ = this;
}

ProfilingGlobals::~ProfilingGlobals() {
  singleton_ = nullptr;
}

base::TaskRunner* ProfilingGlobals::GetIORunner() {
  return message_loop_.task_runner().get();
}

ProfilingProcess* ProfilingGlobals::GetProfilingProcess() {
  return &process_;
}

MemlogConnectionManager* ProfilingGlobals::GetMemlogConnectionManager() {
  return &memlog_connection_manager_;
}

BacktraceStorage* ProfilingGlobals::GetBacktraceStorage() {
  return &backtrace_storage_;
}

void ProfilingGlobals::RunMainMessageLoop() {
  base::RunLoop run_loop;
  run_loop.Run();
}

void ProfilingGlobals::QuitWhenIdle() {
  message_loop_.QuitWhenIdle();
}

}  // namespace profiling
