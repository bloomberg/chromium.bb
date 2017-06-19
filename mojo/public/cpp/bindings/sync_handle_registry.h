// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_

#include <map>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/wait_set.h"

namespace mojo {

// SyncHandleRegistry is a sequence-local storage to register handles that want
// to be watched together.
//
// This class is thread unsafe.
class MOJO_CPP_BINDINGS_EXPORT SyncHandleRegistry
    : public base::RefCounted<SyncHandleRegistry> {
 public:
  // Returns a sequence-local object.
  static scoped_refptr<SyncHandleRegistry> current();

  using HandleCallback = base::Callback<void(MojoResult)>;
  bool RegisterHandle(const Handle& handle,
                      MojoHandleSignals handle_signals,
                      const HandleCallback& callback);

  void UnregisterHandle(const Handle& handle);

  // Registers a |base::WaitableEvent| which can be used to wake up
  // Wait() before any handle signals. |event| is not owned, and if it signals
  // during Wait(), |callback| is invoked. Returns |true| if registered
  // successfully or |false| if |event| was already registered.
  bool RegisterEvent(base::WaitableEvent* event, const base::Closure& callback);

  void UnregisterEvent(base::WaitableEvent* event);

  // Waits on all the registered handles and events and runs callbacks
  // synchronously for any that become ready.
  // The method:
  //   - returns true when any element of |should_stop| is set to true;
  //   - returns false when any error occurs.
  bool Wait(const bool* should_stop[], size_t count);

 private:
  friend class base::RefCounted<SyncHandleRegistry>;

  SyncHandleRegistry();
  ~SyncHandleRegistry();

  WaitSet wait_set_;
  std::map<Handle, HandleCallback> handles_;
  std::map<base::WaitableEvent*, base::Closure> events_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SyncHandleRegistry);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_HANDLE_REGISTRY_H_
