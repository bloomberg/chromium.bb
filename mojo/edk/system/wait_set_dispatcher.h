// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

class MOJO_SYSTEM_IMPL_EXPORT WaitSetDispatcher : public Dispatcher {
 public:
  WaitSetDispatcher();

  // |Dispatcher| public methods:
  Type GetType() const override;

 private:
  // Internal implementation of Awakable.
  class Waiter;

  struct WaitState {
    WaitState();
    ~WaitState();

    scoped_refptr<Dispatcher> dispatcher;
    MojoHandleSignals signals;
    uintptr_t context;
  };

  ~WaitSetDispatcher() override;

  // |Dispatcher| protected methods:
  void CloseImplNoLock() override;
  void CancelAllAwakablesNoLock() override;
  MojoResult AddAwakableImplNoLock(Awakable* awakable,
                                   MojoHandleSignals signals,
                                   uintptr_t context,
                                   HandleSignalsState* signals_state) override;
  void RemoveAwakableImplNoLock(Awakable* awakable,
                                HandleSignalsState* signals_state) override;
  MojoResult AddWaitingDispatcherImplNoLock(
      const scoped_refptr<Dispatcher>& dispatcher,
      MojoHandleSignals signals,
      uintptr_t context) override;
  MojoResult RemoveWaitingDispatcherImplNoLock(
      const scoped_refptr<Dispatcher>& dispatcher) override;
  MojoResult GetReadyDispatchersImplNoLock(uint32_t* count,
                                           DispatcherVector* dispatchers,
                                           MojoResult* results,
                                           uintptr_t* contexts) override;
  HandleSignalsState GetHandleSignalsStateImplNoLock() const override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;

  // Signal that the dispatcher indexed by |context| has been woken up with
  // |result| and is now ready.
  void WakeDispatcher(MojoResult result, uintptr_t context);

  // Map of dispatchers being waited on. Key is a Dispatcher* casted to a
  // uintptr_t, and should be treated as an opaque value and not casted back.
  std::map<uintptr_t, WaitState> waiting_dispatchers_;

  // Separate lock that can be locked without locking |lock()|.
  mutable base::Lock awoken_lock_;
  // List of dispatchers that have been woken up. Any dispatcher in this queue
  // will NOT currently be waited on.
  std::deque<std::pair<uintptr_t, MojoResult>> awoken_queue_;
  // List of dispatchers that have been woken up and retrieved.
  std::deque<uintptr_t> processed_dispatchers_;

  // Separate lock that can be locked without locking |lock()|.
  base::Lock awakable_lock_;
  // List of dispatchers being waited on.
  AwakableList awakable_list_;

  // Waiter used to wait on dispatchers.
  scoped_ptr<Waiter> waiter_;
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_
