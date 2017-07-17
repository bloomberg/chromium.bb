// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SYNC_EVENT_WATCHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SYNC_EVENT_WATCHER_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/sync_handle_registry.h"

namespace mojo {

// SyncEventWatcher supports waiting on a base::WaitableEvent to signal while
// also allowing other SyncEventWatchers and SyncHandleWatchers on the same
// sequence to wake up as needed.
//
// This class is not thread safe.
class MOJO_CPP_BINDINGS_EXPORT SyncEventWatcher {
 public:
  SyncEventWatcher(base::WaitableEvent* event, const base::Closure& callback);

  ~SyncEventWatcher();

  // Registers |event_| with SyncHandleRegistry, so that when others perform
  // sync watching on the same sequence, |event_| will be watched along with
  // them.
  void AllowWokenUpBySyncWatchOnSameThread();

  // Waits on |event_| plus all other events and handles registered with this
  // sequence's SyncHandleRegistry, running callbacks synchronously for any
  // ready events and handles.
  // This method:
  //   - returns true when |should_stop| is set to true;
  //   - return false when any error occurs, including this object being
  //     destroyed during a callback.
  bool SyncWatch(const bool* should_stop);

 private:
  void IncrementRegisterCount();
  void DecrementRegisterCount();

  base::WaitableEvent* const event_;
  const base::Closure callback_;

  // Whether |event_| has been registered with SyncHandleRegistry.
  bool registered_ = false;

  // If non-zero, |event_| should be registered with SyncHandleRegistry.
  size_t register_request_count_ = 0;

  scoped_refptr<SyncHandleRegistry> registry_;

  scoped_refptr<base::RefCountedData<bool>> destroyed_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SyncEventWatcher);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SYNC_EVENT_WATCHER_H_
