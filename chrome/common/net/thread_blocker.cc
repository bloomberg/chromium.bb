// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/thread_blocker.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/waitable_event.h"

// Since a ThreadBlocker is outlived by its target thread, we don't
// have to ref-count it.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chrome_common_net::ThreadBlocker);

namespace chrome_common_net {

ThreadBlocker::ThreadBlocker(base::Thread* target_thread)
    : target_message_loop_(target_thread->message_loop()),
      is_blocked_(false, false), is_finished_blocking_(false, false),
      is_unblocked_(false, false) {
  DCHECK(target_message_loop_);
}

void ThreadBlocker::Block() {
  DCHECK_NE(MessageLoop::current(), target_message_loop_);
  target_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ThreadBlocker::BlockOnTargetThread));
  while (!is_blocked_.Wait()) {}
}

void ThreadBlocker::Unblock() {
  DCHECK_NE(MessageLoop::current(), target_message_loop_);
  is_finished_blocking_.Signal();
  while (!is_unblocked_.Wait()) {}
}

void ThreadBlocker::BlockOnTargetThread() {
  DCHECK_EQ(MessageLoop::current(), target_message_loop_);
  is_blocked_.Signal();
  while (!is_finished_blocking_.Wait()) {}
  is_unblocked_.Signal();
}

}  // namespace chrome_common_net
