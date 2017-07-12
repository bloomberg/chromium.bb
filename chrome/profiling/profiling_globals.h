// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_PROFILING_GLOBALS_H_
#define CHROME_PROFILING_PROFILING_GLOBALS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "chrome/profiling/backtrace_storage.h"
#include "chrome/profiling/memlog_connection_manager.h"
#include "chrome/profiling/profiling_process.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace profiling {

class ProfilingGlobals {
 public:
  ProfilingGlobals();
  ~ProfilingGlobals();

  static inline ProfilingGlobals* Get() { return singleton_; }

  base::TaskRunner* GetIORunner();

  ProfilingProcess* GetProfilingProcess();
  MemlogConnectionManager* GetMemlogConnectionManager();
  BacktraceStorage* GetBacktraceStorage();

  void RunMainMessageLoop();
  void QuitWhenIdle();

 private:
  static ProfilingGlobals* singleton_;

  base::MessageLoopForIO message_loop_;

  ProfilingProcess process_;
  MemlogConnectionManager memlog_connection_manager_;
  BacktraceStorage backtrace_storage_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingGlobals);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_PROFILING_GLOBALS_H_
