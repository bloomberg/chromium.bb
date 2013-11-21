// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/task_runners.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"

namespace mojo {
namespace shell {
namespace {

scoped_ptr<base::Thread> CreateIOThread(const char* name) {
  scoped_ptr<base::Thread> thread(new base::Thread(name));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread->StartWithOptions(options);
  return thread.Pass();
}

}  // namespace

TaskRunners::TaskRunners(base::SingleThreadTaskRunner* ui_runner)
    : ui_runner_(ui_runner),
      cache_thread_(CreateIOThread("cache_thread")),
      io_thread_(CreateIOThread("io_thread")),
      file_thread_(new base::Thread("file_thread")) {
  file_thread_->Start();
}

TaskRunners::~TaskRunners() {
}

}  // namespace shell
}  // namespace mojo
