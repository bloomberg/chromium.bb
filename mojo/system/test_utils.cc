// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/test_utils.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"

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

base::TimeDelta EpsilonTimeout() {
// Originally, our epsilon timeout was 10 ms, which was mostly fine but flaky on
// some Windows bots. I don't recall ever seeing flakes on other bots. At 30 ms
// tests seem reliable on Windows bots, but not at 25 ms. We'd like this timeout
// to be as small as possible (see the description in the .h file).
//
// Currently, |tiny_timeout()| is usually 100 ms (possibly scaled under ASAN,
// etc.). Based on this, set it to (usually be) 30 ms on Windows and 20 ms
// elsewhere.
#if defined(OS_WIN)
  return (TestTimeouts::tiny_timeout() * 3) / 10;
#else
  return (TestTimeouts::tiny_timeout() * 2) / 10;
#endif
}

}  // namespace test
}  // namespace system
}  // namespace mojo
