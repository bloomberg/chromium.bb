// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some notes that may help anyone trying to reason about this code:
//
// - The source thread must be guaranteed to outlive the target
//   thread.  This is so that it is guaranteed that any task posted to
//   the source thread will eventually be run.  In particular, we want
//   to make sure that Detach() causes us to eventually be removed as
//   an observer.
//
//   Note that this implies that any task posted to the target thread
//   from the source thread may not run.  But we post only
//   TargetObserverOnIPAddressChanged() tasks on the target thread,
//   which we can safely drop.
//
// - The source NetworkChangeNotifier must be guaranteed to outlive
//   the target thread.  This is so that it is guaranteed that any
//   task posted to the source thread can safely access the source
//   NetworkChangeNotifier.
//
// - Ref-counting this class is necessary, although as a consequence
//   we can't make any guarantees about which thread will destroy an
//   instance.  Earlier versions of this class tried to get away
//   without ref-counting.  One version deleted the class in
//   Unobserve(); this didn't work because there may still be
//   TargetObserverOnIPAddressChanged() tasks on the target thread.
//   An attempt to fix this was to post a DeleteTask on the target
//   thread from Unobserve(), but this meant that there would be no
//   way of knowing when on the target thread the instance would be
//   deleted.  Indeed, as mentioned above, any tasks posted on the
//   target thread may not run, so this introduced the possibility of
//   a memory leak.
//
// - It is important that all posted tasks that work with a proxy be
//   RunnableMethods so that the ref-counting guarantees that the
//   proxy is still valid when the task runs.

#include "chrome/browser/sync/net/network_change_observer_proxy.h"

#include <cstddef>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"

namespace browser_sync {

NetworkChangeObserverProxy::NetworkChangeObserverProxy(
    MessageLoop* source_message_loop,
    net::NetworkChangeNotifier* source_network_change_notifier,
    MessageLoop* target_message_loop)
    : source_message_loop_(source_message_loop),
      source_network_change_notifier_(source_network_change_notifier),
      target_message_loop_(target_message_loop),
      target_observer_(NULL) {
  DCHECK(source_message_loop_);
  DCHECK(source_network_change_notifier_);
  DCHECK(target_message_loop_);
  DCHECK_NE(source_message_loop_, target_message_loop_);
  DCHECK_EQ(MessageLoop::current(), target_message_loop_);
}

NetworkChangeObserverProxy::~NetworkChangeObserverProxy() {
  MessageLoop* current_message_loop = MessageLoop::current();
  // We can be deleted on either the source or target thread, so the
  // best we can do is check that we're on either.
  DCHECK((current_message_loop == source_message_loop_) ||
         (current_message_loop == target_message_loop_));
  // Even though only the target thread uses target_observer_, it
  // should still be unset even if we're on the source thread; posting
  // a task is effectively a memory barrier.
  DCHECK(!target_observer_);
}

void NetworkChangeObserverProxy::Observe() {
  DCHECK_EQ(MessageLoop::current(), source_message_loop_);
  source_network_change_notifier_->AddObserver(this);
}

// The Task from which this was called may hold the last reference to
// us (this is how we can get deleted on the source thread).
void NetworkChangeObserverProxy::Unobserve() {
  DCHECK_EQ(MessageLoop::current(), source_message_loop_);
  source_network_change_notifier_->RemoveObserver(this);
}

void NetworkChangeObserverProxy::Attach(
    net::NetworkChangeNotifier::Observer* target_observer) {
  DCHECK_EQ(MessageLoop::current(), target_message_loop_);
  DCHECK(!target_observer_);
  target_observer_ = target_observer;
  DCHECK(target_observer_);
  source_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &NetworkChangeObserverProxy::Observe));
}

void NetworkChangeObserverProxy::Detach() {
  DCHECK_EQ(MessageLoop::current(), target_message_loop_);
  DCHECK(target_observer_);
  target_observer_ = NULL;
  source_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &NetworkChangeObserverProxy::Unobserve));
}

// Although we may get this event after Detach() has been called on
// the target thread, we know that Unobserve() hasn't been called yet.
// But we know that it has been posted, so it at least holds a
// reference to us.
void NetworkChangeObserverProxy::OnIPAddressChanged() {
  DCHECK_EQ(MessageLoop::current(), source_message_loop_);
  target_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &NetworkChangeObserverProxy::TargetObserverOnIPAddressChanged));
}

// The Task from which this was called may hold the last reference to
// us (this is how we can get deleted on the target thread).
void NetworkChangeObserverProxy::TargetObserverOnIPAddressChanged() {
  DCHECK_EQ(MessageLoop::current(), target_message_loop_);
  if (target_observer_) {
    target_observer_->OnIPAddressChanged();
  }
}

}  // namespace browser_sync
