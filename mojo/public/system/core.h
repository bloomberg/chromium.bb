// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_CORE_H_
#define MOJO_PUBLIC_SYSTEM_CORE_H_

// Note: This header should be compilable as C.

#include <stdint.h>

#include "mojo/public/system/system_export.h"

// Types -----------------------------------------------------------------------

// TODO(vtl): Notes: Use of undefined flags will lead to undefined behavior
// (typically they'll be ignored), not necessarily an error.

// Handles to Mojo objects.
typedef uint32_t MojoHandle;

// Result codes for Mojo operations. Non-negative values are success codes;
// negative values are error/failure codes.
typedef int32_t MojoResult;

// Used to specify deadlines (timeouts), in microseconds. Note that
// |MOJO_DEADLINE_INDEFINITE| (-1) means "forever".
typedef uint64_t MojoDeadline;

// Used to specify the state of a handle to wait on (e.g., the ability to read
// or write to it).
typedef uint32_t MojoWaitFlags;

// Used to specify different modes to |MojoWriteMessage()|.
typedef uint32_t MojoWriteMessageFlags;

// Used to specify different modes to |MojoReadMessage()|.
typedef uint32_t MojoReadMessageFlags;

// Constants -------------------------------------------------------------------

// |MojoHandle|:
//    |MOJO_HANDLE_INVALID| - A value that is never a valid handle.
#ifdef __cplusplus
const MojoHandle MOJO_HANDLE_INVALID = 0;
#else
#define MOJO_HANDLE_INVALID ((MojoHandle) 0)
#endif

// |MojoResult|:
//    |MOJO_RESULT_OK| - Not an error; returned on success. Note that positive
//        |MojoResult|s may also be used to indicate success.
//    |MOJO_RESULT_CANCELLED| - Operation was cancelled, typically by the
//        caller.
//    |MOJO_RESULT_UNKNOWN| - Unknown error (e.g., if not enough information is
//        available for a more specific error).
//    |MOJO_RESULT_INVALID_ARGUMENT| - Caller specified an invalid argument.
//        This differs from |MOJO_RESULT_FAILED_PRECONDITION| in that the former
//        indicates arguments that are invalid regardless of the state of the
//        system.
//    |MOJO_RESULT_DEADLINE_EXCEEDED| - Deadline expired before the operation
//        could complete.
//    |MOJO_RESULT_NOT_FOUND| - Some requested entity was not found (i.e., does
//        not exist).
//    |MOJO_RESULT_ALREADY_EXISTS| - Some entity or condition that we attempted
//        to create already exists.
//    |MOJO_RESULT_PERMISSION_DENIED| - The caller does not have permission to
//        for the operation (use |MOJO_RESULT_RESOURCE_EXHAUSTED| for rejections
//        caused by exhausting some resource instead).
//    |MOJO_RESULT_RESOURCE_EXHAUSTED| - Some resource required for the call
//        (possibly some quota) has been exhausted.
//    |MOJO_RESULT_FAILED_PRECONDITION| - The system is not in a state required
//        for the operation (use this if the caller must do something to rectify
//        the state before retrying).
//    |MOJO_RESULT_ABORTED| - The operation was aborted by the system, possibly
//        due to a concurrency issue (use this if the caller may retry at a
//        higher level).
//    |MOJO_RESULT_OUT_OF_RANGE| - The operation was attempted past the valid
//        range. Unlike |MOJO_RESULT_INVALID_ARGUMENT|, this indicates that the
//        operation may be/become valid depending on the system state. (This
//        error is similar to |MOJO_RESULT_FAILED_PRECONDITION|, but is more
//        specific.)
//    |MOJO_RESULT_UNIMPLEMENTED| - The operation is not implemented, supported,
//        or enabled.
//    |MOJO_RESULT_INTERNAL| - Internal error: this should never happen and
//        indicates that some invariant expected by the system has been broken.
//    |MOJO_RESULT_UNAVAILABLE| - The operation is (temporarily) currently
//        unavailable. The caller may simply retry the operation (possibly with
//        a backoff).
//    |MOJO_RESULT_DATA_LOSS| - Unrecoverable data loss or corruption.
//
// Note that positive values are also available as success codes.
//
// The codes from |MOJO_RESULT_OK| to |MOJO_RESULT_DATA_LOSS| come from
// Google3's canonical error codes.
#ifdef __cplusplus
const MojoResult MOJO_RESULT_OK = 0;
const MojoResult MOJO_RESULT_CANCELLED = -1;
const MojoResult MOJO_RESULT_UNKNOWN = -2;
const MojoResult MOJO_RESULT_INVALID_ARGUMENT = -3;
const MojoResult MOJO_RESULT_DEADLINE_EXCEEDED = -4;
const MojoResult MOJO_RESULT_NOT_FOUND = -5;
const MojoResult MOJO_RESULT_ALREADY_EXISTS = -6;
const MojoResult MOJO_RESULT_PERMISSION_DENIED = -7;
const MojoResult MOJO_RESULT_RESOURCE_EXHAUSTED = -8;
const MojoResult MOJO_RESULT_FAILED_PRECONDITION = -9;
const MojoResult MOJO_RESULT_ABORTED = -10;
const MojoResult MOJO_RESULT_OUT_OF_RANGE = -11;
const MojoResult MOJO_RESULT_UNIMPLEMENTED = -12;
const MojoResult MOJO_RESULT_INTERNAL = -13;
const MojoResult MOJO_RESULT_UNAVAILABLE = -14;
const MojoResult MOJO_RESULT_DATA_LOSS = -15;
#else
#define MOJO_RESULT_OK ((MojoResult) 0)
#define MOJO_RESULT_CANCELLED ((MojoResult) -1)
#define MOJO_RESULT_UNKNOWN ((MojoResult) -2)
#define MOJO_RESULT_INVALID_ARGUMENT ((MojoResult) -3)
#define MOJO_RESULT_DEADLINE_EXCEEDED ((MojoResult) -4)
#define MOJO_RESULT_NOT_FOUND ((MojoResult) -5)
#define MOJO_RESULT_ALREADY_EXISTS ((MojoResult) -6)
#define MOJO_RESULT_PERMISSION_DENIED ((MojoResult) -7)
#define MOJO_RESULT_RESOURCE_EXHAUSTED ((MojoResult) -8)
#define MOJO_RESULT_FAILED_PRECONDITION ((MojoResult) -9)
#define MOJO_RESULT_ABORTED ((MojoResult) -10)
#define MOJO_RESULT_OUT_OF_RANGE ((MojoResult) -11)
#define MOJO_RESULT_UNIMPLEMENTED ((MojoResult) -12)
#define MOJO_RESULT_INTERNAL ((MojoResult) -13)
#define MOJO_RESULT_UNAVAILABLE ((MojoResult) -14)
#define MOJO_RESULT_DATA_LOSS ((MojoResult) -15)
#endif

// |MojoDeadline|:
//    |MOJO_DEADLINE_INDEFINITE|
#ifdef __cplusplus
const MojoDeadline MOJO_DEADLINE_INDEFINITE = static_cast<MojoDeadline>(-1);
#else
#define MOJO_DEADLINE_INDEFINITE = ((MojoDeadline) -1);
#endif

// |MojoWaitFlags|:
//    |MOJO_WAIT_FLAG_NONE| - No flags. |MojoWait()|, etc. will return
//        |MOJO_RESULT_FAILED_PRECONDITION| if you attempt to wait on this.
//    |MOJO_WAIT_FLAG_READABLE| - Can read (e.g., a message) from the handle.
//    |MOJO_WAIT_FLAG_WRITABLE| - Can write (e.g., a message) to the handle.
//    |MOJO_WAIT_FLAG_EVERYTHING| - All flags.
#ifdef __cplusplus
const MojoWaitFlags MOJO_WAIT_FLAG_NONE = 0;
const MojoWaitFlags MOJO_WAIT_FLAG_READABLE = 1 << 0;
const MojoWaitFlags MOJO_WAIT_FLAG_WRITABLE = 1 << 1;
const MojoWaitFlags MOJO_WAIT_FLAG_EVERYTHING = ~0;
#else
#define MOJO_WAIT_FLAG_NONE ((MojoWaitFlags) 0)
#define MOJO_WAIT_FLAG_READABLE ((MojoWaitFlags) 1 << 0)
#define MOJO_WAIT_FLAG_WRITABLE ((MojoWaitFlags) 1 << 1)
#define MOJO_WAIT_FLAG_EVERYTHING (~((MojoWaitFlags) 0))
#endif

// |MojoWriteMessageFlags|:
//    |MOJO_WRITE_MESSAGE_FLAG_NONE| - No flags; default mode.
#ifdef __cplusplus
const MojoWriteMessageFlags MOJO_WRITE_MESSAGE_FLAG_NONE = 0;
#else
#define MOJO_WRITE_MESSAGE_FLAG_NONE ((MojoWriteMessageFlags) 0)
#endif

// |MojoReadMessageFlags|:
//    |MOJO_READ_MESSAGE_FLAG_NONE| - No flags; default mode.
//    |MOJO_READ_MESSAGE_FLAG_MAY_DISCARD| - If the message is unable to be read
//        for whatever reason (e.g., the caller-supplied buffer is too small),
//        discard the message (i.e., simply dequeue it).
#ifdef __cplusplus
const MojoReadMessageFlags MOJO_READ_MESSAGE_FLAG_NONE = 0;
const MojoReadMessageFlags MOJO_READ_MESSAGE_FLAG_MAY_DISCARD = 1 << 0;
#else
#define MOJO_READ_MESSAGE_FLAG_NONE ((MojoReadMessageFlags) 0)
#define MOJO_READ_MESSAGE_FLAG_MAY_DISCARD ((MojoReadMessageFlags) 1 << 0)
#endif

// Functions -------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Closes the given |handle|.
//
// Returns:
//    |MOJO_RESULT_OK| on success.
//    |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle.
//
// Concurrent operations on |handle| may succeed (or fail as usual) if they
// happen before the close, be cancelled with result |MOJO_RESULT_CANCELLED| if
// they properly overlap (this is likely the case with |MojoWait()|, etc.), or
// fail with |MOJO_RESULT_INVALID_ARGUMENT| if they happen after.
MOJO_SYSTEM_EXPORT MojoResult MojoClose(MojoHandle handle);

// Waits on the given handle until the state indicated by |flags| is satisfied
// or until |deadline| has passed.
//
// Returns:
//    |MOJO_RESULT_OK| if some flag in |flags| was satisfied (or is already
//        satisfied).
//    |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle (e.g., if
//        it has already been closed).
//    |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//        the flags being satisfied.
//    |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that any
//        flag in |flags| will ever be satisfied.
//
// If there are multiple waiters (on different threads, obviously) waiting on
// the same handle and flag and that flag becomes set, all waiters will be
// awoken.
MOJO_SYSTEM_EXPORT MojoResult MojoWait(MojoHandle handle,
                                       MojoWaitFlags flags,
                                       MojoDeadline deadline);

// Waits on |handles[0]|, ..., |handles[num_handles-1]| for at least one of them
// to satisfy the state indicated by |flags[0]|, ..., |flags[num_handles-1]|,
// respectively, or until |deadline| has passed.
//
// Returns:
//    The index |i| (from 0 to |num_handles-1|) if |handle[i]| satisfies
//        |flags[i]|.
//    |MOJO_RESULT_INVALID_ARGUMENT| if some |handle[i]| is not a valid handle
//        (e.g., if it has already been closed).
//    |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//        handles satisfying any of its flags.
//    |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that SOME
//        |handle[i]| will ever satisfy any of its flags |flags[i]|.
MOJO_SYSTEM_EXPORT MojoResult MojoWaitMany(const MojoHandle* handles,
                                           const MojoWaitFlags* flags,
                                           uint32_t num_handles,
                                           MojoDeadline deadline);

// TODO(vtl): flags? other params (e.g., queue sizes, max message sizes?)
MOJO_SYSTEM_EXPORT MojoResult MojoCreateMessagePipe(MojoHandle* handle_0,
                                                    MojoHandle* handle_1);

MOJO_SYSTEM_EXPORT MojoResult MojoWriteMessage(
    MojoHandle handle,
    const void* bytes, uint32_t num_bytes,
    const MojoHandle* handles,
    uint32_t num_handles,
    MojoWriteMessageFlags flags);

MOJO_SYSTEM_EXPORT MojoResult MojoReadMessage(MojoHandle handle,
                                              void* bytes, uint32_t* num_bytes,
                                              MojoHandle* handles,
                                              uint32_t* num_handles,
                                              MojoReadMessageFlags flags);

#ifdef __cplusplus
}  // extern "C"
#endif

// C++ wrapper functions -------------------------------------------------------

#ifdef __cplusplus

namespace mojo {

// Used to assert things at compile time. (Use our own copy instead of
// Chromium's, since we can't depend on Chromium.)
template <bool> struct CompileAssert {};
#define MOJO_COMPILE_ASSERT(expr, msg) \
    typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]

struct Handle { MojoHandle value; };

const Handle kInvalidHandle = { MOJO_HANDLE_INVALID };

// A |mojo::Handle| must take no extra space, since we'll treat arrays of them
// as if they were arrays of |MojoHandle|s.
MOJO_COMPILE_ASSERT(sizeof(Handle) == sizeof(MojoHandle),
                    bad_size_for_cplusplus_handle);

inline MojoResult Close(Handle handle) {
  return MojoClose(handle.value);
}

inline MojoResult Wait(Handle handle,
                       MojoWaitFlags flags,
                       MojoDeadline deadline) {
  return MojoWait(handle.value, flags, deadline);
}

inline MojoResult WaitMany(const Handle* handles,
                           const MojoWaitFlags* flags,
                           uint32_t num_handles,
                           MojoDeadline deadline) {
  return MojoWaitMany(&handles[0].value, flags, num_handles, deadline);
}

inline MojoResult CreateMessagePipe(Handle* handle_0, Handle* handle_1) {
  return MojoCreateMessagePipe(&handle_0->value, &handle_1->value);
}

inline MojoResult WriteMessage(Handle handle,
                               const void* bytes, uint32_t num_bytes,
                               const Handle* handles, uint32_t num_handles,
                               MojoWriteMessageFlags flags) {
  return MojoWriteMessage(handle.value,
                          bytes, num_bytes,
                          &handles[0].value, num_handles,
                          flags);
}

inline MojoResult ReadMessage(Handle handle,
                              void* bytes, uint32_t* num_bytes,
                              Handle* handles, uint32_t* num_handles,
                              MojoReadMessageFlags flags) {
  return MojoReadMessage(handle.value,
                         bytes, num_bytes,
                         &handles[0].value, num_handles,
                         flags);
}

}  // namespace mojo
#endif

#endif  // MOJO_PUBLIC_SYSTEM_CORE_H_
