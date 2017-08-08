// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/watch.h"

#include "mojo/edk/system/request_context.h"
#include "mojo/edk/system/watcher_dispatcher.h"

namespace mojo {
namespace edk {

Watch::Watch(const scoped_refptr<WatcherDispatcher>& watcher,
             const scoped_refptr<Dispatcher>& dispatcher,
             uintptr_t context,
             MojoHandleSignals signals,
             MojoWatchCondition condition)
    : watcher_(watcher),
      dispatcher_(dispatcher),
      context_(context),
      signals_(signals),
      condition_(condition) {}

bool Watch::NotifyState(const HandleSignalsState& state,
                        bool allowed_to_call_callback) {
  AssertWatcherLockAcquired();

  // TODO(crbug.com/740044): Remove this CHECK.
  CHECK(this);

  // NOTE: This method must NEVER call into |dispatcher_| directly, because it
  // may be called while |dispatcher_| holds a lock.

  MojoResult rv = MOJO_RESULT_SHOULD_WAIT;
  RequestContext* const request_context = RequestContext::current();
  const bool notify_success =
      (state.satisfies_any(signals_) &&
       condition_ == MOJO_WATCH_CONDITION_SATISFIED) ||
      (!state.satisfies_all(signals_) &&
       condition_ == MOJO_WATCH_CONDITION_NOT_SATISFIED);
  if (notify_success) {
    rv = MOJO_RESULT_OK;
    if (allowed_to_call_callback && rv != last_known_result_) {
      request_context->AddWatchNotifyFinalizer(this, MOJO_RESULT_OK, state);
    }
  } else if (condition_ == MOJO_WATCH_CONDITION_SATISFIED &&
             !state.can_satisfy_any(signals_)) {
    rv = MOJO_RESULT_FAILED_PRECONDITION;
    if (allowed_to_call_callback && rv != last_known_result_) {
      request_context->AddWatchNotifyFinalizer(
          this, MOJO_RESULT_FAILED_PRECONDITION, state);
    }
  }

  last_known_signals_state_ =
      *static_cast<const MojoHandleSignalsState*>(&state);
  last_known_result_ = rv;
  return ready();
}

void Watch::Cancel() {
  // TODO(crbug.com/740044): Remove this CHECK.
  CHECK(this);

  RequestContext::current()->AddWatchCancelFinalizer(this);
}

void Watch::InvokeCallback(MojoResult result,
                           const HandleSignalsState& state,
                           MojoWatcherNotificationFlags flags) {
  // We hold the lock through invocation to ensure that only one notification
  // callback runs for this context at any given time.
  base::AutoLock lock(notification_lock_);
  if (result == MOJO_RESULT_CANCELLED) {
    // Make sure cancellation is the last notification we dispatch.
    DCHECK(!is_cancelled_);
    is_cancelled_ = true;
  } else if (is_cancelled_) {
    return;
  }

  // NOTE: This will acquire |watcher_|'s internal lock. It's safe because a
  // thread can only enter InvokeCallback() from within a RequestContext
  // destructor where no dispatcher locks are held.
  watcher_->InvokeWatchCallback(context_, result, state, flags);
}

Watch::~Watch() {}

#if DCHECK_IS_ON()
void Watch::AssertWatcherLockAcquired() const {
  watcher_->lock_.AssertAcquired();
}
#endif

}  // namespace edk
}  // namespace mojo
