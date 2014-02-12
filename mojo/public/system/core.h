// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_CORE_H_
#define MOJO_PUBLIC_SYSTEM_CORE_H_

// Note: This header should be compilable as C.

#include <stdint.h>

#include "mojo/public/system/macros.h"
#include "mojo/public/system/system_export.h"

// Types/constants -------------------------------------------------------------

// TODO(vtl): Notes: Use of undefined flags will lead to undefined behavior
// (typically they'll be ignored), not necessarily an error.

// |MojoTimeTicks|: Used to specify time ticks. Value is in microseconds.

typedef int64_t MojoTimeTicks;

// |MojoHandle|: Handles to Mojo objects.
//   |MOJO_HANDLE_INVALID| - A value that is never a valid handle.

typedef uint32_t MojoHandle;

#ifdef __cplusplus
const MojoHandle MOJO_HANDLE_INVALID = 0;
#else
#define MOJO_HANDLE_INVALID ((MojoHandle) 0)
#endif

// |MojoResult|: Result codes for Mojo operations. Non-negative values are
// success codes; negative values are error/failure codes.
//   |MOJO_RESULT_OK| - Not an error; returned on success. Note that positive
//       |MojoResult|s may also be used to indicate success.
//   |MOJO_RESULT_CANCELLED| - Operation was cancelled, typically by the caller.
//   |MOJO_RESULT_UNKNOWN| - Unknown error (e.g., if not enough information is
//       available for a more specific error).
//   |MOJO_RESULT_INVALID_ARGUMENT| - Caller specified an invalid argument. This
//       differs from |MOJO_RESULT_FAILED_PRECONDITION| in that the former
//       indicates arguments that are invalid regardless of the state of the
//       system.
//   |MOJO_RESULT_DEADLINE_EXCEEDED| - Deadline expired before the operation
//       could complete.
//   |MOJO_RESULT_NOT_FOUND| - Some requested entity was not found (i.e., does
//       not exist).
//   |MOJO_RESULT_ALREADY_EXISTS| - Some entity or condition that we attempted
//       to create already exists.
//   |MOJO_RESULT_PERMISSION_DENIED| - The caller does not have permission to
//       for the operation (use |MOJO_RESULT_RESOURCE_EXHAUSTED| for rejections
//       caused by exhausting some resource instead).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| - Some resource required for the call
//       (possibly some quota) has been exhausted.
//   |MOJO_RESULT_FAILED_PRECONDITION| - The system is not in a state required
//       for the operation (use this if the caller must do something to rectify
//       the state before retrying).
//   |MOJO_RESULT_ABORTED| - The operation was aborted by the system, possibly
//       due to a concurrency issue (use this if the caller may retry at a
//       higher level).
//   |MOJO_RESULT_OUT_OF_RANGE| - The operation was attempted past the valid
//       range. Unlike |MOJO_RESULT_INVALID_ARGUMENT|, this indicates that the
//       operation may be/become valid depending on the system state. (This
//       error is similar to |MOJO_RESULT_FAILED_PRECONDITION|, but is more
//       specific.)
//   |MOJO_RESULT_UNIMPLEMENTED| - The operation is not implemented, supported,
//       or enabled.
//   |MOJO_RESULT_INTERNAL| - Internal error: this should never happen and
//       indicates that some invariant expected by the system has been broken.
//   |MOJO_RESULT_UNAVAILABLE| - The operation is (temporarily) currently
//       unavailable. The caller may simply retry the operation (possibly with a
//       backoff).
//   |MOJO_RESULT_DATA_LOSS| - Unrecoverable data loss or corruption.
//   |MOJO_RESULT_BUSY| - One of the resources involved is currently being used
//       (possibly on another thread) in a way that prevents the current
//       operation from proceeding, e.g., if the other operation may result in
//       the resource being invalidated.
//   |MOJO_RESULT_SHOULD_WAIT| - The request cannot currently be completed
//       (e.g., if the data requested is not yet available). The caller should
//       wait for it to be feasible using |MojoWait()| or |MojoWaitMany()|.
//
// Note that positive values are also available as success codes.
//
// The codes from |MOJO_RESULT_OK| to |MOJO_RESULT_DATA_LOSS| come from
// Google3's canonical error codes.
//
// TODO(vtl): Add a |MOJO_RESULT_UNSATISFIABLE|?

typedef int32_t MojoResult;

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
const MojoResult MOJO_RESULT_BUSY = -16;
const MojoResult MOJO_RESULT_SHOULD_WAIT = -17;
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
#define MOJO_RESULT_BUSY ((MojoResult) -16)
#define MOJO_RESULT_SHOULD_WAIT ((MojoResult) -17)
#endif

// |MojoDeadline|: Used to specify deadlines (timeouts), in microseconds (except
// for |MOJO_DEADLINE_INDEFINITE|).
//   |MOJO_DEADLINE_INDEFINITE| - Used to indicate "forever".

typedef uint64_t MojoDeadline;

#ifdef __cplusplus
const MojoDeadline MOJO_DEADLINE_INDEFINITE = static_cast<MojoDeadline>(-1);
#else
#define MOJO_DEADLINE_INDEFINITE ((MojoDeadline) -1)
#endif

// |MojoWaitFlags|: Used to specify the state of a handle to wait on (e.g., the
// ability to read or write to it).
//   |MOJO_WAIT_FLAG_NONE| - No flags. |MojoWait()|, etc. will return
//       |MOJO_RESULT_FAILED_PRECONDITION| if you attempt to wait on this.
//   |MOJO_WAIT_FLAG_READABLE| - Can read (e.g., a message) from the handle.
//   |MOJO_WAIT_FLAG_WRITABLE| - Can write (e.g., a message) to the handle.
//   |MOJO_WAIT_FLAG_EVERYTHING| - All flags.

typedef uint32_t MojoWaitFlags;

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

// Message pipe:

// |MojoWriteMessageFlags|: Used to specify different modes to
// |MojoWriteMessage()|.
//   |MOJO_WRITE_MESSAGE_FLAG_NONE| - No flags; default mode.

typedef uint32_t MojoWriteMessageFlags;

#ifdef __cplusplus
const MojoWriteMessageFlags MOJO_WRITE_MESSAGE_FLAG_NONE = 0;
#else
#define MOJO_WRITE_MESSAGE_FLAG_NONE ((MojoWriteMessageFlags) 0)
#endif

// |MojoReadMessageFlags|: Used to specify different modes to
// |MojoReadMessage()|.
//   |MOJO_READ_MESSAGE_FLAG_NONE| - No flags; default mode.
//   |MOJO_READ_MESSAGE_FLAG_MAY_DISCARD| - If the message is unable to be read
//       for whatever reason (e.g., the caller-supplied buffer is too small),
//       discard the message (i.e., simply dequeue it).

typedef uint32_t MojoReadMessageFlags;

#ifdef __cplusplus
const MojoReadMessageFlags MOJO_READ_MESSAGE_FLAG_NONE = 0;
const MojoReadMessageFlags MOJO_READ_MESSAGE_FLAG_MAY_DISCARD = 1 << 0;
#else
#define MOJO_READ_MESSAGE_FLAG_NONE ((MojoReadMessageFlags) 0)
#define MOJO_READ_MESSAGE_FLAG_MAY_DISCARD ((MojoReadMessageFlags) 1 << 0)
#endif

// Data pipe:

// |MojoCreateDataPipeOptions|: Used to specify creation parameters for a data
// pipe to |MojoCreateDataPipe()|.
//   |uint32_t struct_size|: Set to the size of the |MojoCreateDataPipeOptions|
//       struct. (Used to allow for future extensions.)
//   |MojoCreateDataPipeOptionsFlags flags|: Used to specify different modes of
//       operation.
//     |MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE|: No flags; default mode.
//     |MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD|: May discard data for
//         whatever reason; best-effort delivery. In particular, if the capacity
//         is reached, old data may be discard to make room for new data.
//   |uint32_t element_num_bytes|: The size of an element, in bytes. All
//       transactions and buffers will consist of an integral number of
//       elements. Must be nonzero.
//   |uint32_t capacity_num_bytes|: The capacity of the data pipe, in number of
//       bytes; must be a multiple of |element_num_bytes|. The data pipe will
//       always be able to queue AT LEAST this much data. Set to zero to opt for
//       a system-dependent automatically-calculated capacity (which will always
//       be at least one element).

typedef uint32_t MojoCreateDataPipeOptionsFlags;

#ifdef __cplusplus
const MojoCreateDataPipeOptionsFlags
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE = 0;
const MojoCreateDataPipeOptionsFlags
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD = 1 << 0;
#else
#define MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE \
    ((MojoCreateDataPipeOptionsFlags) 0)
#define MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD \
    ((MojoCreateDataPipeOptionsFlags) 1 << 0)
#endif

struct MojoCreateDataPipeOptions {
  uint32_t struct_size;
  MojoCreateDataPipeOptionsFlags flags;
  uint32_t element_num_bytes;
  uint32_t capacity_num_bytes;
};
// TODO(vtl): Can we make this assertion work in C?
#ifdef __cplusplus
MOJO_COMPILE_ASSERT(sizeof(MojoCreateDataPipeOptions) == 16,
                    MojoCreateDataPipeOptions_has_wrong_size);
#endif

// |MojoWriteDataFlags|: Used to specify different modes to |MojoWriteData()|
// and |MojoBeginWriteData()|.
//   |MOJO_WRITE_DATA_FLAG_NONE| - No flags; default mode.
//   |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| - Write either all the elements
//       requested or none of them.

typedef uint32_t MojoWriteDataFlags;

#ifdef __cplusplus
const MojoWriteDataFlags MOJO_WRITE_DATA_FLAG_NONE = 0;
const MojoWriteDataFlags MOJO_WRITE_DATA_FLAG_ALL_OR_NONE = 1 << 0;
#else
#define MOJO_WRITE_DATA_FLAG_NONE ((MojoWriteDataFlags) 0)
#define MOJO_WRITE_DATA_FLAG_ALL_OR_NONE ((MojoWriteDataFlags) 1 << 0)
#endif

// |MojoReadDataFlags|: Used to specify different modes to |MojoReadData()| and
// |MojoBeginReadData()|.
//   |MOJO_READ_DATA_FLAG_NONE| - No flags; default mode.
//   |MOJO_READ_DATA_FLAG_ALL_OR_NONE| - Read (or discard) either the requested
//        number of elements or none.
//   |MOJO_READ_DATA_FLAG_DISCARD| - Discard (up to) the requested number of
//        elements.
//   |MOJO_READ_DATA_FLAG_QUERY| - Query the number of elements available to
//       read. For use with |MojoReadData()| only. Mutually exclusive with
//       |MOJO_READ_DATA_FLAG_DISCARD| and |MOJO_READ_DATA_FLAG_ALL_OR_NONE| is
//       ignored if this flag is set.

typedef uint32_t MojoReadDataFlags;

#ifdef __cplusplus
const MojoReadDataFlags MOJO_READ_DATA_FLAG_NONE = 0;
const MojoReadDataFlags MOJO_READ_DATA_FLAG_ALL_OR_NONE = 1 << 0;
const MojoReadDataFlags MOJO_READ_DATA_FLAG_DISCARD = 1 << 1;
const MojoReadDataFlags MOJO_READ_DATA_FLAG_QUERY = 1 << 2;
#else
#define MOJO_READ_DATA_FLAG_NONE ((MojoReadDataFlags) 0)
#define MOJO_READ_DATA_FLAG_ALL_OR_NONE ((MojoReadDataFlags) 1 << 0)
#define MOJO_READ_DATA_FLAG_DISCARD ((MojoReadDataFlags) 1 << 1)
#define MOJO_READ_DATA_FLAG_QUERY ((MojoReadDataFlags) 1 << 2)
#endif

// Shared buffer:

// |MojoCreateSharedBufferOptions|: Used to specify creation parameters for a
// shared buffer to |MojoCreateSharedBuffer()|.
//   |uint32_t struct_size|: Set to the size of the
//       |MojoCreateSharedBufferOptions| struct. (Used to allow for future
//       extensions.)
//   |MojoCreateSharedBufferOptionsFlags flags|: Reserved for future use.
//       |MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE|: No flags; default mode.
//
// TODO(vtl): Maybe add a flag to indicate whether the memory should be
// executable or not?
// TODO(vtl): Also a flag for discardable (ashmem-style) buffers.

typedef uint32_t MojoCreateSharedBufferOptionsFlags;

#ifdef __cplusplus
const MojoCreateSharedBufferOptionsFlags
    MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE = 0;
#else
#define MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE \
    ((MojoCreateSharedBufferOptionsFlags) 0)
#endif

struct MojoCreateSharedBufferOptions {
  uint32_t struct_size;
  MojoCreateSharedBufferOptionsFlags flags;
};
// TODO(vtl): Can we make this assertion work in C?
#ifdef __cplusplus
MOJO_COMPILE_ASSERT(sizeof(MojoCreateSharedBufferOptions) == 8,
                    MojoCreateSharedBufferOptions_has_wrong_size);
#endif

// |MojoDuplicateBufferHandleOptions|: Used to specify parameters in duplicating
// access to a shared buffer to |MojoDuplicateBufferHandle()|.
//   |uint32_t struct_size|: Set to the size of the
//       |MojoDuplicateBufferHandleOptions| struct. (Used to allow for future
//       extensions.)
//   |MojoDuplicateBufferHandleOptionsFlags flags|: Reserved for future use.
//       |MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE|: No flags; default
//       mode.
//
// TODO(vtl): Add flags to remove writability (and executability)? Also, COW?

typedef uint32_t MojoDuplicateBufferHandleOptionsFlags;

#ifdef __cplusplus
const MojoDuplicateBufferHandleOptionsFlags
    MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE = 0;
#else
#define MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE \
    ((MojoDuplicateBufferHandleOptionsFlags) 0)
#endif

struct MojoDuplicateBufferHandleOptions {
  uint32_t struct_size;
  MojoDuplicateBufferHandleOptionsFlags flags;
};
// TODO(vtl): Can we make this assertion work in C?
#ifdef __cplusplus
MOJO_COMPILE_ASSERT(sizeof(MojoDuplicateBufferHandleOptions) == 8,
                    MojoDuplicateBufferHandleOptions_has_wrong_size);
#endif

// |MojoMapBufferFlags|: Used to specify different modes to |MojoMapBuffer()|.
//   |MOJO_MAP_BUFFER_FLAG_NONE| - No flags; default mode.

typedef uint32_t MojoMapBufferFlags;

#ifdef __cplusplus
const MojoMapBufferFlags MOJO_MAP_BUFFER_FLAG_NONE = 0;
#else
#define MOJO_MAP_BUFFER_FLAG_NONE ((MojoMapBufferFlags) 0)
#endif

// Functions -------------------------------------------------------------------

// Note: Pointer parameters that are labeled "optional" may be null (at least
// under some circumstances). Non-const pointer parameters are also labelled
// "in", "out", or "in/out", to indicate how they are used. (Note that how/if
// such a parameter is used may depend on other parameters or the requested
// operation's success/failure. E.g., a separate |flags| parameter may control
// whether a given "in/out" parameter is used for input, output, or both.)

#ifdef __cplusplus
extern "C" {
#endif

// Platform-dependent monotonically increasing tick count representing "right
// now." The resolution of this clock is ~1-15ms.  Resolution varies depending
// on hardware/operating system configuration.
MOJO_SYSTEM_EXPORT MojoTimeTicks MojoGetTimeTicksNow();

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

// Waits on the given handle until the state indicated by |flags| is satisfied
// or until |deadline| has passed.
//
// Returns:
//   |MOJO_RESULT_OK| if some flag in |flags| was satisfied (or is already
//        satisfied).
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle (e.g., if
//       it has already been closed).
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       the flags being satisfied.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that any
//       flag in |flags| will ever be satisfied.
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
//   The index |i| (from 0 to |num_handles-1|) if |handle[i]| satisfies
//       |flags[i]|.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some |handle[i]| is not a valid handle
//       (e.g., if it has already been closed).
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       handles satisfying any of its flags.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that SOME
//       |handle[i]| will ever satisfy any of its flags |flags[i]|.
MOJO_SYSTEM_EXPORT MojoResult MojoWaitMany(const MojoHandle* handles,
                                           const MojoWaitFlags* flags,
                                           uint32_t num_handles,
                                           MojoDeadline deadline);

// Message pipe:

// Creates a message pipe, which is a bidirectional communication channel for
// framed data (i.e., messages). Messages can contain plain data and/or Mojo
// handles. On success, |*message_pipe_handle0| and |*message_pipe_handle1| are
// set to handles for the two endpoints (ports) for the message pipe.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message_pipe_handle0| and/or
//       |message_pipe_handle1| do not appear to be valid pointers.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached.
//
// TODO(vtl): Add an options struct pointer argument.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateMessagePipe(
    MojoHandle* message_pipe_handle0,  // Out.
    MojoHandle* message_pipe_handle1);  // Out.

// Writes a message to the message pipe endpoint given by |message_pipe_handle|,
// with message data specified by |bytes| of size |num_bytes| and attached
// handles specified by |handles| of count |num_handles|, and options specified
// by |flags|. If there is no message data, |bytes| may be null, in which case
// |num_bytes| must be zero. If there are no attached handles, |handles| may be
// null, in which case |num_handles| must be zero.
//
// If handles are attached, on success the handles will no longer be valid (the
// receiver will receive equivalent, but logically different, handles). Handles
// to be sent should not be in simultaneous use (e.g., on another thread).
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., the message was enqueued).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g., if
//       |message_pipe_handle| is not a valid handle, or some of the
//       requirements above are not satisfied).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if some system limit has been reached, or
//       the number of handles to send is too large (TODO(vtl): reconsider the
//       latter case).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed.
//       Note that closing an endpoint is not necessarily synchronous (e.g.,
//       across processes), so this function may be succeed even if the other
//       endpoint has been closed (in which case the message would be dropped).
//   |MOJO_RESULT_BUSY| if some handle to be sent is currently in use.
//
// TODO(vtl): Add a notion of capacity for message pipes, and return
// |MOJO_RESULT_SHOULD_WAIT| if the message pipe is full.
MOJO_SYSTEM_EXPORT MojoResult MojoWriteMessage(
    MojoHandle message_pipe_handle,
    const void* bytes,  // Optional.
    uint32_t num_bytes,
    const MojoHandle* handles,  // Optional.
    uint32_t num_handles,
    MojoWriteMessageFlags flags);

// Reads a message from the message pipe endpoint given by
// |message_pipe_handle|; also usable to query the size of the next message or
// discard the next message. |bytes|/|*num_bytes| indicate the buffer/buffer
// size to receive the message data (if any) and |handles|/|*num_handles|
// indicate the buffer/maximum handle count to receive the attached handles (if
// any).
//
// |num_bytes| and |num_handles| are optional "in-out" parameters. If non-null,
// on return |*num_bytes| and |*num_handles| will usually indicate the number
// of bytes and number of attached handles in the "next" message, respectively,
// whether that message was read or not. (If null, the number of bytes/handles
// is treated as zero.)
//
// If |bytes| is null, then |*num_bytes| must be zero, and similarly for
// |handles| and |*num_handles|.
//
// Partial reads are NEVER done. Either a full read is done and |MOJO_RESULT_OK|
// returned, or the read is NOT done and |MOJO_RESULT_RESOURCE_EXHAUSTED| is
// returned (if |MOJO_READ_MESSAGE_FLAG_MAY_DISCARD| was set, the message is
// also discarded in this case).
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., a message was actually read).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if one of the buffers to receive the
//       message/attached handles (|bytes|/|*num_bytes| or
//       |handles|/|*num_handles|) was too small. (TODO(vtl): Reconsider this
//       error code; should distinguish this from the hitting-system-limits
//       case.)
//   |MOJO_RESULT_SHOULD_WAIT| if no message was available to be read.
MOJO_SYSTEM_EXPORT MojoResult MojoReadMessage(
    MojoHandle message_pipe_handle,
    void* bytes,  // Optional out.
    uint32_t* num_bytes,  // Optional in/out.
    MojoHandle* handles,  // Optional out.
    uint32_t* num_handles,  // Optional in/out.
    MojoReadMessageFlags flags);

// Data pipe:

// Creates a data pipe, which is a unidirectional communication channel for
// unframed data, with the given options. Data is unframed, but must come as
// (multiples of) discrete elements, of the size given in |options|. See
// |MojoCreateDataPipeOptions| for a description of the different options
// available for data pipes.
//
// |options| may be set to null for a data pipe with the default options (which
// will have an element size of one byte and have some system-dependent
// capacity).
//
// On success, |*data_pipe_producer_handle| will be set to the handle for the
// producer and |*data_pipe_consumer_handle| will be set to the handle for the
// consumer. (On failure, they are not modified.)
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g., if
//       |*options| is invalid or one of the pointer handles looks invalid).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached (e.g., if the requested capacity was too large, or if the
//       maximum number of handles was exceeded).
MOJO_SYSTEM_EXPORT MojoResult MojoCreateDataPipe(
    const struct MojoCreateDataPipeOptions* options,  // Optional.
    MojoHandle* data_pipe_producer_handle,  // Out.
    MojoHandle* data_pipe_consumer_handle);  // Out.

// Writes the given data to the data pipe producer given by
// |data_pipe_producer_handle|. |elements| points to data of size |*num_bytes|;
// |*num_bytes| should be a multiple of the data pipe's element size. If
// |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| is set in |flags|, either all the data
// will be written or none is.
//
// On success, |*num_bytes| is set to the amount of data that was actually
// written.
//
// Note: If the data pipe has the "may discard" option flag (specified on
// creation), this will discard as much data as required to write the given
// data, starting with the earliest written data that has not been consumed.
// However, even with "may discard", if |*num_bytes| is greater than the data
// pipe's capacity (and |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| is not set), this
// will write the maximum amount possible (namely, the data pipe's capacity) and
// set |*num_bytes| to that amount. It will *not* discard data from |elements|.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g., if
//       |data_pipe_producer_dispatcher| is not a handle to a data pipe
//       producer, |elements| does not look like a valid pointer, or
//       |*num_bytes| is not a multiple of the data pipe's element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer handle has been
//       closed.
//   |MOJO_RESULT_OUT_OF_RANGE| if |flags| has
//       |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set and the required amount of data
//       (specified by |*num_bytes|) could not be written.
//   |MOJO_RESULT_BUSY| if there is a two-phase write ongoing with
//       |data_pipe_producer_handle| (i.e., |MojoBeginWriteData()| has been
//       called, but not yet the matching |MojoEndWriteData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be written (and the
//       consumer is still open) and |flags| does *not* have
//       |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set.
//
// TODO(vtl): Should there be a way of querying how much data can be written?
MOJO_SYSTEM_EXPORT MojoResult MojoWriteData(
    MojoHandle data_pipe_producer_handle,
    const void* elements,
    uint32_t* num_bytes,  // In/out.
    MojoWriteDataFlags flags);

// Begins a two-phase write to the data pipe producer given by
// |data_pipe_producer_handle|. On success, |*buffer| will be a pointer to which
// the caller can write |*buffer_num_bytes| bytes of data. If flags has
// |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set, then the output value
// |*buffer_num_bytes| will be at least as large as its input value, which must
// also be a multiple of the element size (if |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE|
// is not set, the input value of |*buffer_num_bytes| is ignored).
//
// During a two-phase write, |data_pipe_producer_handle| is *not* writable.
// E.g., if another thread tries to write to it, it will get |MOJO_RESULT_BUSY|;
// that thread can then wait for |data_pipe_producer_handle| to become writable
// again.
//
// Once the caller has finished writing data to |*buffer|, it should call
// |MojoEndWriteData()| to specify the amount written and to complete the
// two-phase write.
//
// Note: If the data pipe has the "may discard" option flag (specified on
// creation) and |flags| has |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set, this may
// discard some data.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g., if
//       |data_pipe_producer_handle| is not a handle to a data pipe producer,
//       |buffer| or |buffer_num_bytes| does not look like a valid pointer, or
//       flags has |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set and
//       |*buffer_num_bytes| is not a multiple of the element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer handle has been
//       closed.
//   |MOJO_RESULT_OUT_OF_RANGE| if |flags| has
//       |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set and the required amount of data
//       (specified by |*buffer_num_bytes|) cannot be written contiguously at
//       this time. (Note that there may be space available for the required
//       amount of data, but the "next" write position may not be large enough.)
//   |MOJO_RESULT_BUSY| if there is already a two-phase write ongoing with
//       |data_pipe_producer_handle| (i.e., |MojoBeginWriteData()| has been
//       called, but not yet the matching |MojoEndWriteData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be written (and the
//       consumer is still open).
MOJO_SYSTEM_EXPORT MojoResult MojoBeginWriteData(
    MojoHandle data_pipe_producer_handle,
    void** buffer,  // Out.
    uint32_t* buffer_num_bytes,  // In/out.
    MojoWriteDataFlags flags);

// Ends a two-phase write to the data pipe producer given by
// |data_pipe_producer_handle| that was begun by a call to
// |MojoBeginWriteData()| on the same handle. |num_bytes_written| should
// indicate the amount of data actually written; it must be less than or equal
// to the value of |*buffer_num_bytes| output by |MojoBeginWriteData()| and must
// be a multiple of the element size. The buffer given by |*buffer| from
// |MojoBeginWriteData()| must have been filled with exactly |num_bytes_written|
// bytes of data.
//
// On failure, the two-phase write (if any) is ended (so the handle may become
// writable again, if there's space available) but no data written to |*buffer|
// is "put into" the data pipe.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |data_pipe_producer_handle| is not a
//       handle to a data pipe producer or |num_bytes_written| is invalid
//       (greater than the maximum value provided by |MojoBeginWriteData()| or
//       not a multiple of the element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer is not in a
//       two-phase write (e.g., |MojoBeginWriteData()| was not called or
//       |MojoEndWriteData()| has already been called).
MOJO_SYSTEM_EXPORT MojoResult MojoEndWriteData(
    MojoHandle data_pipe_producer_handle,
    uint32_t num_bytes_written);

// Reads data from the data pipe consumer given by |data_pipe_consumer_handle|.
// May also be used to discard data or query the amount of data available.
//
// If |flags| has neither |MOJO_READ_DATA_FLAG_DISCARD| nor
// |MOJO_READ_DATA_FLAG_QUERY| set, this tries to read up to |*num_bytes| (which
// must be a multiple of the data pipe's element size) bytes of data to
// |elements| and set |*num_bytes| to the amount actually read. If flags has
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set, it will either read exactly
// |*num_bytes| bytes of data or none.
//
// If flags has |MOJO_READ_DATA_FLAG_DISCARD| set, it discards up to
// |*num_bytes| (which again be a multiple of the element size) bytes of data,
// setting |*num_bytes| to the amount actually discarded. If flags has
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE|, it will either discard exactly
// |*num_bytes| bytes of data or none. In this case, |MOJO_READ_DATA_FLAG_QUERY|
// must not be set, and |elements| is ignored (and should typically be set to
// null).
//
// If flags has |MOJO_READ_DATA_FLAG_QUERY| set, it queries the amount of data
// available, setting |*num_bytes| to the number of bytes available. In this
// case, |MOJO_READ_DATA_FLAG_DISCARD| must not be set, and
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| is ignored, as are |elements| and the input
// value of |*num_bytes|.
//
// Returns:
//   |MOJO_RESULT_OK| on success (see above for a description of the different
//       operations).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_consumer_handle| is invalid, the combination of flags in
//       |flags| is invalid, etc.).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer handle has been
//       closed and data (or the required amount of data) was not available to
//       be read or discarded.
//   |MOJO_RESULT_OUT_OF_RANGE| if |flags| has |MOJO_READ_DATA_FLAG_ALL_OR_NONE|
//       set and the required amount of data is not available to be read or
//       discarded (and the producer is still open).
//   |MOJO_RESULT_BUSY| if there is a two-phase read ongoing with
//       |data_pipe_consumer_handle| (i.e., |MojoBeginReadData()| has been
//       called, but not yet the matching |MojoEndReadData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if there is no data to be read or discarded (and
//       the producer is still open) and |flags| does *not* have
//       |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set.
MOJO_SYSTEM_EXPORT MojoResult MojoReadData(
    MojoHandle data_pipe_consumer_handle,
    void* elements,  // Out.
    uint32_t* num_bytes,  // In/out.
    MojoReadDataFlags flags);

// Begins a two-phase read from the data pipe consumer given by
// |data_pipe_consumer_handle|. On success, |*buffer| will be a pointer from
// which the caller can read |*buffer_num_bytes| bytes of data. If flags has
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set, then the output value
// |*buffer_num_bytes| will be at least as large as its input value, which must
// also be a multiple of the element size (if |MOJO_READ_DATA_FLAG_ALL_OR_NONE|
// is not set, the input value of |*buffer_num_bytes| is ignored). |flags| must
// not have |MOJO_READ_DATA_FLAG_DISCARD| or |MOJO_READ_DATA_FLAG_QUERY| set.
//
// During a two-phase read, |data_pipe_consumer_handle| is *not* readable.
// E.g., if another thread tries to read from it, it will get
// |MOJO_RESULT_BUSY|; that thread can then wait for |data_pipe_consumer_handle|
// to become readable again.
//
// Once the caller has finished reading data from |*buffer|, it should call
// |MojoEndReadData()| to specify the amount read and to complete the two-phase
// read.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g., if
//       |data_pipe_consumer_handle| is not a handle to a data pipe consumer,
//       |buffer| or |buffer_num_bytes| does not look like a valid pointer,
//       |flags| has |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set and
//       |*buffer_num_bytes| is not a multiple of the element size, or |flags|
//       has invalid flags set).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer handle has been
//       closed.
//   |MOJO_RESULT_OUT_OF_RANGE| if |flags| has |MOJO_READ_DATA_FLAG_ALL_OR_NONE|
//       set and the required amount of data (specified by |*buffer_num_bytes|)
//       cannot be read from a contiguous buffer at this time. (Note that there
//       may be the required amount of data, but it may not be contiguous.)
//   |MOJO_RESULT_BUSY| if there is already a two-phase read ongoing with
//       |data_pipe_consumer_handle| (i.e., |MojoBeginReadData()| has been
//       called, but not yet the matching |MojoEndReadData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be read (and the
//       producer is still open).
MOJO_SYSTEM_EXPORT MojoResult MojoBeginReadData(
    MojoHandle data_pipe_consumer_handle,
    const void** buffer,  // Out.
    uint32_t* buffer_num_bytes,  // In/out.
    MojoReadDataFlags flags);

// Ends a two-phase read from the data pipe consumer given by
// |data_pipe_consumer_handle| that was begun by a call to |MojoBeginReadData()|
// on the same handle. |num_bytes_read| should indicate the amount of data
// actually read; it must be less than or equal to the value of
// |*buffer_num_bytes| output by |MojoBeginReadData()| and must be a multiple of
// the element size.
//
// On failure, the two-phase read (if any) is ended (so the handle may become
// readable again) but no data is "removed" from the data pipe.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |data_pipe_consumer_handle| is not a
//       handle to a data pipe consumer or |num_bytes_written| is invalid
//       (greater than the maximum value provided by |MojoBeginReadData()| or
//       not a multiple of the element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer is not in a
//       two-phase read (e.g., |MojoBeginReadData()| was not called or
//       |MojoEndReadData()| has already been called).
MOJO_SYSTEM_EXPORT MojoResult MojoEndReadData(
    MojoHandle data_pipe_consumer_handle,
    uint32_t num_bytes_read);

// Shared buffer:

// TODO(vtl): General comments.

// Creates a buffer that can be shared between applications (by duplicating the
// handle -- see |MojoDuplicateBufferHandle()| -- and passing it over a message
// pipe). To access the buffer, one must call |MojoMapBuffer()|.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateSharedBuffer(
    const struct MojoCreateSharedBufferOptions* options,  // Optional.
    uint64_t* num_bytes,  // In/out.
    MojoHandle* shared_buffer_handle);  // Out.

// Duplicates the handle |buffer_handle| to a buffer. This creates another
// handle (returned in |*new_buffer_handle| on success), which can then be sent
// to another application over a message pipe, while retaining access to the
// |buffer_handle| (and any mappings that it may have).
//
// Note: We may add buffer types for which this operation is not supported.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,  // Optional.
    MojoHandle* new_buffer_handle);  // Out.

// Map the part (at offset |offset| of length |num_bytes|) of the buffer given
// by |buffer_handle| into memory. |offset + num_bytes| must be less than or
// equal to the size of the buffer. On success, |*buffer| points to memory with
// the requested part of the buffer.
//
// A single buffer handle may have multiple active mappings (possibly depending
// on the buffer type). The permissions (e.g., writable or executable) of the
// returned memory may depend on the properties of the buffer and properties
// attached to the buffer handle as well as |flags|.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                                            uint64_t offset,
                                            uint64_t num_bytes,
                                            void** buffer,  // Out.
                                            MojoMapBufferFlags flags);

// Unmap a buffer pointer that was mapped by |MojoMapBuffer()|.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoUnmapBuffer(void* buffer);  // In.

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_SYSTEM_CORE_H_
