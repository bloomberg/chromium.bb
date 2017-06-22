// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_SYSTEM_WATCHER_H_
#define MOJO_PUBLIC_C_SYSTEM_WATCHER_H_

#include <stdint.h>

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// A callback used to notify watchers about events on their watched handles.
//
// See documentation for |MojoWatcherNotificationFlags| for details regarding
// the possible values of |flags|.
//
// See documentation for |MojoWatch()| for details regarding the other arguments
// this callback receives when called.
typedef void (*MojoWatcherCallback)(uintptr_t context,
                                    MojoResult result,
                                    struct MojoHandleSignalsState signals_state,
                                    MojoWatcherNotificationFlags flags);

// Creates a new watcher.
//
// Watchers are used to trigger arbitrary code execution when one or more
// handles change state to meet certain conditions.
//
// A newly registered watcher is initially disarmed and may be armed using
// |MojoArmWatcher()|. A watcher is also always disarmed immediately before any
// invocation of one or more notification callbacks in response to a single
// handle's state changing in some relevant way.
//
// Parameters:
//   |callback|: The |MojoWatcherCallback| to invoke any time the watcher is
//       notified of an event. See |MojoWatch()| for details regarding arguments
//       passed to the callback. Note that this may be called from any arbitrary
//       thread.
//   |watcher_handle|: The address at which to store the MojoHandle
//       corresponding to the new watcher if successfully created.
//
// Returns:
//   |MOJO_RESULT_OK| if the watcher has been successfully created.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a handle could not be allocated for
//       this watcher.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateWatcher(MojoWatcherCallback callback,
                                                MojoHandle* watcher_handle);

// Adds a watch to a watcher. This allows the watcher to fire notifications
// regarding state changes on the handle corresponding to the arguments given.
//
// Note that notifications for a given watch context are guaranteed to be
// mutually exclusive in execution: the callback will never be entered for a
// given context while another invocation of the callback is still executing for
// the same context. As a result it is generally a good idea to ensure that
// callbacks do as little work as necessary in order to process the
// notification.
//
// Parameters:
//   |watcher_handle|: The watcher to which |handle| is to be added.
//   |handle|: The handle to add to the watcher.
//   |signals|: The signals to watch for on |handle|.
//   |level|: The level to watch for on |signals|.
//   |context|: An arbitrary context value given to any invocation of the
//       watcher's callback when invoked as a result of some state change
//       relevant to this combination of |handle| and |signals|. Must be
//       unique within any given watcher.
//
// Callback parameters (see |MojoWatcherNotificationCallback| above):
//   When the watcher invokes its callback as a result of some notification
//   relevant to this watch operation, |context| receives the value given here
//   and |signals_state| receives the last known signals state of this handle.
//
//   |result| is one of the following:
//     |MOJO_RESULT_OK| if at least one of the watched signals is satisfied. The
//         watcher must be armed for this notification to fire.
//     |MOJO_RESULT_FAILED_PRECONDITION| if all of the watched signals are
//         permanently unsatisfiable. The watcher must be armed for this
//         notification to fire.
//     |MOJO_RESULT_CANCELLED| if the watch has been cancelled. The may occur if
//         the watcher has been closed, the watched handle has been closed, or
//         the watch for |context| has been explicitly cancelled. This is always
//         the last result received for any given context, and it is guaranteed
//         to be received exactly once per watch, regardless of how the watch
//         was cancelled.
//
// Returns:
//   |MOJO_RESULT_OK| if the handle is now being watched by the watcher.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |watcher_handle| is not a watcher handle,
//       |handle| is not a valid message pipe or data pipe handle.
//   |MOJO_RESULT_ALREADY_EXISTS| if the watcher already has a watch registered
//       for the given value of |context| or for the given |handle|.
MOJO_SYSTEM_EXPORT MojoResult MojoWatch(MojoHandle watcher_handle,
                                        MojoHandle handle,
                                        MojoHandleSignals signals,
                                        MojoWatchCondition condition,
                                        uintptr_t context);

// Removes a watch from a watcher.
//
// This ensures that the watch is cancelled as soon as possible. Cancellation
// may be deferred (or may even block) an aritrarily long time if the watch is
// already dispatching one or more notifications.
//
// When cancellation is complete, the watcher's callback is invoked one final
// time for |context|, with the result |MOJO_RESULT_CANCELLED|.
//
// The same behavior can be elicted by either closing the watched handle
// associated with this context, or by closing |watcher_handle| itself. In the
// lastter case, all registered contexts on the watcher are implicitly cancelled
// in a similar fashion.
//
// Parameters:
//   |watcher_handle|: The handle of the watcher from which to remove a watch.
//   |context|: The context of the watch to be removed.
//
// Returns:
//   |MOJO_RESULT_OK| if the watch has been cancelled.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |watcher_handle| is not a watcher handle.
//   |MOJO_RESULT_NOT_FOUND| if there is no watch registered on this watcher for
//       the given value of |context|.
MOJO_SYSTEM_EXPORT MojoResult MojoCancelWatch(MojoHandle watcher_handle,
                                              uintptr_t context);

// Arms a watcher, enabling a single future event on one of the watched handles
// to trigger a single notification for each relevant watch context associated
// with that handle.
//
// Parameters:
//   |watcher_handle|: The handle of the watcher.
//   |num_ready_contexts|: An address pointing to the number of elements
//       available for storage in the remaining output buffers. Optional and
//       only used on failure. See |MOJO_RESULT_FAILED_PRECONDITION| below for
//       more details.
//   |ready_contexts|: An output buffer for contexts corresponding to the
//       watches which would have notified if the watcher were armed. Optional
//       and only uesd on failure. See |MOJO_RESULT_FAILED_PRECONDITION| below
//       for more details.
//   |ready_results|: An output buffer for MojoResult values corresponding to
//       each context in |ready_contexts|. Optional and only used on failure.
//       See |MOJO_RESULT_FAILED_PRECONDITION| below for more details.
//   |ready_signals_states|: An output buffer for |MojoHandleSignalsState|
//       structures corresponding to each context in |ready_contexts|. Optional
//       and only used on failure. See |MOJO_RESULT_FAILED_PRECONDITION| below
//       for more details.
//
// Returns:
//   |MOJO_RESULT_OK| if the watcher has been successfully armed. All arguments
//       other than |watcher_handle| are ignored in this case.
//   |MOJO_RESULT_NOT_FOUND| if the watcher does not have any registered watch
//       contexts. All arguments other than |watcher_handle| are ignored in this
//       case.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |watcher_handle| is not a valid watcher
//       handle, or if |num_ready_contexts| is non-null but any of the output
//       buffer paramters is null.
//   |MOJO_RESULT_FAILED_PRECONDITION| if one or more watches would have
//       notified immediately upon arming the watcher. If |num_handles| is
//       non-null, this assumes there is enough space for |*num_handles| entries
//       in each of the subsequent output buffer arguments.
//
//       At most that many entries are placed in the output buffers,
//       corresponding to the watches which would have signalled if the watcher
//       had been armed successfully. The actual number of entries placed in the
//       output buffers is written to |*num_ready_contexts| before returning.
//
//       If more than (input) |*num_ready_contexts| watch contexts were ready to
//       notify, the subset presented in output buffers is arbitrary, but the
//       implementation makes a best effort to circulate the outputs across
//       consecutive calls so that callers may reliably avoid handle starvation.
MOJO_SYSTEM_EXPORT MojoResult
MojoArmWatcher(MojoHandle watcher_handle,
               uint32_t* num_ready_contexts,
               uintptr_t* ready_contexts,
               MojoResult* ready_results,
               struct MojoHandleSignalsState* ready_signals_states);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_WATCHER_H_
