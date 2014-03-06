// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/test_utils.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"

namespace mojo {
namespace system {
namespace test {

namespace {

void PostTaskAndWaitHelper(base::WaitableEvent* event,
                           const base::Closure& task) {
  task.Run();
  event->Signal();
}

}  // namespace

void PostTaskAndWait(scoped_refptr<base::TaskRunner> task_runner,
                     const tracked_objects::Location& from_here,
                     const base::Closure& task) {
  base::WaitableEvent event(false, false);
  task_runner->PostTask(from_here,
                        base::Bind(&PostTaskAndWaitHelper, &event, task));
  event.Wait();
}

// TestIOThread ----------------------------------------------------------------

TestIOThread::TestIOThread() : io_thread_("test_io_thread") {
  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
}

TestIOThread::~TestIOThread() {
  io_thread_.Stop();
}

}  // namespace test
}  // namespace system
}  // namespace mojo
