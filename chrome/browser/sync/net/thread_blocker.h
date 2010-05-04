// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NET_THREAD_BLOCKER_H_
#define CHROME_BROWSER_SYNC_NET_THREAD_BLOCKER_H_

// This class (mainly used for testing) lets you block and unblock a
// thread at will.
//
// TODO(akalin): Consider moving this to base/ as this class is not
// sync-specific (ask darin about it).

#include "base/basictypes.h"
#include "base/waitable_event.h"

class MessageLoop;

namespace base {
class Thread;
}  // namespace base

namespace browser_sync {

class ThreadBlocker {
 public:
  // The given thread must already be started and it must outlive this
  // instance.
  explicit ThreadBlocker(base::Thread* target_thread);

  // When this function returns, the target thread will be blocked
  // until Unblock() is called.  Each call to Block() must be matched
  // by a call to Unblock().
  void Block();

  // When this function returns, the target thread is unblocked.
  void Unblock();

 private:
  // On the target thread, blocks until Unblock() is called.
  void BlockOnTargetThread();

  MessageLoop* const target_message_loop_;
  base::WaitableEvent is_blocked_, is_finished_blocking_, is_unblocked_;

  DISALLOW_COPY_AND_ASSIGN(ThreadBlocker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NET_THREAD_BLOCKER_H_
