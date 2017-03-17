// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_WAIT_SET_H_
#define MOJO_PUBLIC_CPP_SYSTEM_WAIT_SET_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// WaitSet provides an efficient means of blocking a thread on any number of
// Mojo handles changing state in some interesting way.
//
// Unlike WaitMany(), which incurs some extra setup cost for every call, a
// WaitSet maintains some persistent accounting of a set of handles which can be
// added or removed from the set. A blocking wait operation (see the Wait()
// method below) can then be performed multiple times for the same set of
// handles with minimal additional setup per call.
//
// WaitSet is NOT thread-safe, so naturally handles may not be added to or
// removed from the set while waiting.
class MOJO_CPP_SYSTEM_EXPORT WaitSet {
 public:
  WaitSet();
  ~WaitSet();

  // Adds |handle| to the set of handles to wait on. If successful, any future
  // Wait() on this WaitSet will wake up in the event that one or more signals
  // in |signals| becomes satisfied on |handle| or all of them become
  // permanently unsatisfiable.
  //
  // Return values:
  //   |MOJO_RESULT_OK| if |handle| has been successfully added.
  //   |MOJO_RESULT_ALREADY_EXISTS| if |handle| is already in this WaitSet.
  //   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle.
  MojoResult AddHandle(Handle handle, MojoHandleSignals signals);

  // Removes |handle| from the set of handles to wait on. Future calls to
  // Wait() will be unaffected by the state of this handle.
  //
  // Return values:
  //   |MOJO_RESULT_OK| if |handle| has been successfully removed.
  //   |MOJO_RESULT_NOT_FOUND| if |handle| was not in the set.
  MojoResult RemoveHandle(Handle handle);

  // Waits on the current set of handles for one or more of them to meet the
  // conditions which were specified when they were added via AddHandle() above.
  //
  // |*num_ready_handles| on input must specify the number of entries available
  // for output storage in |ready_handles| and |ready_result| (which must both
  // be non-null). If |signals_states| is non-null it must also point to enough
  // storage for |*num_ready_handles| MojoHandleSignalsState structures.
  //
  // Upon return, |*num_ready_handles| will contain the total number of handles
  // whose information is stored in the given output buffers.
  //
  // Every entry in |ready_handles| on output corresponds to one of the handles
  // whose signaling state termianted the Wait() operation. Every corresponding
  // entry in |ready_results| indicates the status of a ready handle according
  // to the following result codes:
  //   |MOJO_RESULT_OK| one or more signals for the handle has been satisfied.
  //   |MOJO_RESULT_FAILED_PRECONDITION| all of the signals for the handle have
  //       become permanently unsatisfiable.
  //   |MOJO_RESULT_CANCELLED| if the handle has been closed from another
  //       thread. NOTE: It is important to recognize that this means the
  //       corresponding value in |ready_handles| is either invalid, or valid
  //       but referring to a different handle (i.e. has already been reused) by
  //       the time Wait() returns. The handle in question is automatically
  //       removed from the WaitSet.
  void Wait(size_t* num_ready_handles,
            Handle* ready_handles,
            MojoResult* ready_results,
            MojoHandleSignalsState* signals_states = nullptr);

 private:
  class State;

  // Thread-safe state associated with this WaitSet. Used to aggregate
  // notifications from watched handles.
  scoped_refptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(WaitSet);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_WAIT_SET_H_
