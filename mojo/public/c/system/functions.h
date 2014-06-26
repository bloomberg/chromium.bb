// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains basic functions common to different Mojo system APIs.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
#define MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_

// Note: This header should be compilable as C.

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Note: Pointer parameters that are labelled "optional" may be null (at least
// under some circumstances). Non-const pointer parameters are also labeled
// "in", "out", or "in/out", to indicate how they are used. (Note that how/if
// such a parameter is used may depend on other parameters or the requested
// operation's success/failure. E.g., a separate |flags| parameter may control
// whether a given "in/out" parameter is used for input, output, or both.)

// Platform-dependent monotonically increasing tick count representing "right
// now." The resolution of this clock is ~1-15ms.  Resolution varies depending
// on hardware/operating system configuration.
MOJO_SYSTEM_EXPORT MojoTimeTicks MojoGetTimeTicksNow(void);

// Closes the given |handle|.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle.
//
// Concurrent operations on |handle| may succeed (or fail as usual) if they
// happen before the close, be cancelled with result |MOJO_RESULT_CANCELLED| if
// they properly overlap (this is likely the case with |MojoWait()|, etc.), or
// fail with |MOJO_RESULT_INVALID_ARGUMENT| if they happen after.
MOJO_SYSTEM_EXPORT MojoResult MojoClose(MojoHandle handle);

// Waits on the given handle until a signal indicated by |signals| is satisfied,
// it becomes known that no signal indicated by |signals| will ever be satisfied
// (see the description of the |MOJO_RESULT_CANCELLED| and
// |MOJO_RESULT_FAILED_PRECONDITION| return values below), or until |deadline|
// has passed.
//
// If |deadline| is |MOJO_DEADLINE_INDEFINITE|, this will wait "forever" (until
// one of the other wait termination conditions is satisfied). If |deadline| is
// 0, this will return |MOJO_RESULT_DEADLINE_EXCEEDED| only if one of the other
// termination conditions (e.g., a signal is satisfied, or all signals are
// unsatisfiable) is not already satisfied.
//
// Returns:
//   |MOJO_RESULT_OK| if some signal in |signals| was satisfied (or is already
//       satisfied).
//   |MOJO_RESULT_CANCELLED| if |handle| was closed (necessarily from another
//       thread) during the wait.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle (e.g., if
//       it has already been closed).
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       the signals being satisfied.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it becomes known that none of the
//       signals in |signals| can ever be satisfied (e.g., when waiting on one
//       end of a message pipe and the other end is closed).
//
// If there are multiple waiters (on different threads, obviously) waiting on
// the same handle and signal, and that signal becomes is satisfied, all waiters
// will be awoken.
MOJO_SYSTEM_EXPORT MojoResult MojoWait(MojoHandle handle,
                                       MojoHandleSignals signals,
                                       MojoDeadline deadline);

// Waits on |handles[0]|, ..., |handles[num_handles-1]| until (at least) one
// satisfies a signal indicated in its respective |signals[0]|, ...,
// |signals[num_handles-1]|, it becomes known that no signal in some
// |signals[i]| will ever be satisfied, or until |deadline| has passed.
//
// This means that |MojoWaitMany()| behaves as if |MojoWait()| were called on
// each handle/signals pair simultaneously, completing when the first
// |MojoWait()| would complete.
//
// See |MojoWait()| for more details about |deadline|.
//
// Returns:
//   The index |i| (from 0 to |num_handles-1|) if |handle[i]| satisfies a signal
//       from |signals[i]|.
//   |MOJO_RESULT_CANCELLED| if some |handle[i]| was closed (necessarily from
//       another thread) during the wait.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some |handle[i]| is not a valid handle
//       (e.g., if it has already been closed).
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       handles satisfying any of its signals.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that SOME
//       |handle[i]| will ever satisfy any of the signals in |signals[i]|.
MOJO_SYSTEM_EXPORT MojoResult MojoWaitMany(const MojoHandle* handles,
                                           const MojoHandleSignals* signals,
                                           uint32_t num_handles,
                                           MojoDeadline deadline);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
