// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/wait.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/c/system/watcher.h"
#include "mojo/public/cpp/system/watcher.h"

namespace mojo {
namespace {

class WatchContext : public base::RefCountedThreadSafe<WatchContext> {
 public:
  WatchContext()
      : event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  base::WaitableEvent& event() { return event_; }
  MojoResult wait_result() const { return wait_result_; }
  MojoHandleSignalsState wait_state() const { return wait_state_; }
  uintptr_t context_value() const { return reinterpret_cast<uintptr_t>(this); }

  static void OnNotification(uintptr_t context_value,
                             MojoResult result,
                             MojoHandleSignalsState state,
                             MojoWatcherNotificationFlags flags) {
    auto* context = reinterpret_cast<WatchContext*>(context_value);
    context->Notify(result, state);
    if (result == MOJO_RESULT_CANCELLED) {
      // Balanced in Wait() or WaitMany().
      context->Release();
    }
  }

 private:
  friend class base::RefCountedThreadSafe<WatchContext>;

  ~WatchContext() {}

  void Notify(MojoResult result, MojoHandleSignalsState state) {
    if (wait_result_ == MOJO_RESULT_UNKNOWN) {
      wait_result_ = result;
      wait_state_ = state;
    }
    event_.Signal();
  }

  base::WaitableEvent event_;

  // NOTE: Although these are modified in Notify() which may be called from any
  // thread, Notify() is guaranteed to never run concurrently with itself.
  // Furthermore, they are only modified once, before |event_| signals; so there
  // is no need for a WatchContext user to synchronize access to these fields
  // apart from waiting on |event()|.
  MojoResult wait_result_ = MOJO_RESULT_UNKNOWN;
  MojoHandleSignalsState wait_state_ = {0, 0};

  DISALLOW_COPY_AND_ASSIGN(WatchContext);
};

}  // namespace

MojoResult Wait(Handle handle,
                MojoHandleSignals signals,
                MojoWatchCondition condition,
                MojoHandleSignalsState* signals_state) {
  ScopedWatcherHandle watcher;
  MojoResult rv = CreateWatcher(&WatchContext::OnNotification, &watcher);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  scoped_refptr<WatchContext> context = new WatchContext;

  // Balanced in WatchContext::OnNotification if MojoWatch() is successful.
  // Otherwise balanced immediately below.
  context->AddRef();

  rv = MojoWatch(watcher.get().value(), handle.value(), signals, condition,
                 context->context_value());
  if (rv == MOJO_RESULT_INVALID_ARGUMENT) {
    // Balanced above.
    context->Release();
    return rv;
  }
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  uint32_t num_ready_contexts = 1;
  uintptr_t ready_context;
  MojoResult ready_result;
  MojoHandleSignalsState ready_state;
  rv = MojoArmWatcher(watcher.get().value(), &num_ready_contexts,
                      &ready_context, &ready_result, &ready_state);
  if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    DCHECK_EQ(1u, num_ready_contexts);
    if (signals_state)
      *signals_state = ready_state;
    return ready_result;
  }

  // Wait for the first notification only.
  context->event().Wait();

  ready_result = context->wait_result();
  DCHECK_NE(MOJO_RESULT_UNKNOWN, ready_result);

  if (signals_state)
    *signals_state = context->wait_state();

  return ready_result;
}

MojoResult WaitMany(const Handle* handles,
                    const MojoHandleSignals* signals,
                    size_t num_handles,
                    size_t* result_index,
                    MojoHandleSignalsState* signals_states) {
  if (!handles || !signals)
    return MOJO_RESULT_INVALID_ARGUMENT;

  ScopedWatcherHandle watcher;
  MojoResult rv = CreateWatcher(&WatchContext::OnNotification, &watcher);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  std::vector<scoped_refptr<WatchContext>> contexts(num_handles);
  std::vector<base::WaitableEvent*> events(num_handles);
  for (size_t i = 0; i < num_handles; ++i) {
    contexts[i] = new WatchContext();

    // Balanced in WatchContext::OnNotification if MojoWatch() is successful.
    // Otherwise balanced immediately below.
    contexts[i]->AddRef();

    MojoResult rv =
        MojoWatch(watcher.get().value(), handles[i].value(), signals[i],
                  MOJO_WATCH_CONDITION_SATISFIED, contexts[i]->context_value());
    if (rv == MOJO_RESULT_INVALID_ARGUMENT) {
      if (result_index)
        *result_index = i;

      // Balanced above.
      contexts[i]->Release();

      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    events[i] = &contexts[i]->event();
  }

  uint32_t num_ready_contexts = 1;
  uintptr_t ready_context = 0;
  MojoResult ready_result = MOJO_RESULT_UNKNOWN;
  MojoHandleSignalsState ready_state{0, 0};
  rv = MojoArmWatcher(watcher.get().value(), &num_ready_contexts,
                      &ready_context, &ready_result, &ready_state);

  size_t index = num_handles;
  if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    DCHECK_EQ(1u, num_ready_contexts);

    // Most commonly we only watch a small number of handles. Just scan for
    // the right index.
    for (size_t i = 0; i < num_handles; ++i) {
      if (contexts[i]->context_value() == ready_context) {
        index = i;
        break;
      }
    }
  } else {
    DCHECK_EQ(MOJO_RESULT_OK, rv);

    // Wait for one of the contexts to signal. First one wins.
    index = base::WaitableEvent::WaitMany(events.data(), events.size());
    ready_result = contexts[index]->wait_result();
    ready_state = contexts[index]->wait_state();
  }

  DCHECK_NE(MOJO_RESULT_UNKNOWN, ready_result);
  DCHECK_LT(index, num_handles);

  if (result_index)
    *result_index = index;

  if (signals_states) {
    for (size_t i = 0; i < num_handles; ++i) {
      if (i == index) {
        signals_states[i] = ready_state;
      } else {
        signals_states[i] = handles[i].QuerySignalsState();
      }
    }
  }

  return ready_result;
}

}  // namespace mojo
