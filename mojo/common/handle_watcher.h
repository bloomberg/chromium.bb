// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_HANDLE_WATCHER_H_
#define MOJO_COMMON_HANDLE_WATCHER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/system/core_cpp.h"

namespace base {
class Thread;
class TickClock;
class TimeTicks;
}

namespace mojo {
namespace common {
namespace test {
class HandleWatcherTest;
}

// HandleWatcher is used to asynchronously wait on a handle and notify a Closure
// when the handle is ready, or the deadline has expired.
class MOJO_COMMON_EXPORT HandleWatcher {
 public:
  HandleWatcher();
  ~HandleWatcher();

  // Starts listening for |handle|. This implicitly invokes Stop(). In other
  // words, Start() performs one asynchronous watch at a time. It is ok to call
  // Start() multiple times, but it cancels any existing watches. |callback| is
  // notified when the handle is ready, invalid or deadline has passed and is
  // notified on the thread Start() was invoked on.
  void Start(const Handle& handle,
             MojoWaitFlags wait_flags,
             MojoDeadline deadline,
             const base::Callback<void(MojoResult)>& callback);

  // Stops listening. Does nothing if not in the process of listening.
  void Stop();

  // Returns now. Used internally; generally not useful.
  static base::TimeTicks NowTicks();

  // Converts a MojoDeadline into a TimeTicks.
  static base::TimeTicks MojoDeadlineToTimeTicks(MojoDeadline deadline);

 private:
  friend class test::HandleWatcherTest;
  struct StartState;

  // See description of |StartState::weak_factory| for details.
  void OnHandleReady(MojoResult result);

  // If non-NULL Start() has been invoked.
  scoped_ptr<StartState> start_state_;

  // Used for getting the time. Only set by tests.
  static base::TickClock* tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(HandleWatcher);
};

}  // namespace common
}  // namespace mojo

#endif  // MOJO_COMMON_HANDLE_WATCHER_H_
