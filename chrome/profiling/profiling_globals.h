// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_PROFILING_GLOBALS_H_
#define CHROME_PROFILING_PROFILING_GLOBALS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"

namespace base {
class MessageLoopForUI;
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

namespace profiling {

class ProfilingGlobals {
 public:
  static ProfilingGlobals* Get();

  base::TaskRunner* GetIORunner();

  // Returns non-null when inside RunMainMessageLoop. Call only on the
  // main thread (otherwise there's a shutdown race).
  scoped_refptr<base::SingleThreadTaskRunner> GetMainThread() const;

  void RunMainMessageLoop();
  void QuitWhenIdle();

 private:
  ProfilingGlobals();
  ~ProfilingGlobals();

  // Non-null inside RunMainMessageLoop.
  base::MessageLoopForUI* main_message_loop_ = nullptr;

  base::Thread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingGlobals);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_PROFILING_GLOBALS_H_
