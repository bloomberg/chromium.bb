// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_SYSTEM_CORE_H_
#define MOJO_PUBLIC_C_SYSTEM_CORE_H_

// Note: This header should be compilable as C.

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Types/constants -------------------------------------------------------------

// TODO(vtl): Split these into their own header files.

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
MOJO_COMPILE_ASSERT(sizeof(MojoCreateDataPipeOptions) == 16,
                    MojoCreateDataPipeOptions_has_wrong_size);

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
MOJO_COMPILE_ASSERT(sizeof(MojoCreateSharedBufferOptions) == 8,
                    MojoCreateSharedBufferOptions_has_wrong_size);

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
MOJO_COMPILE_ASSERT(sizeof(MojoDuplicateBufferHandleOptions) == 8,
                    MojoDuplicateBufferHandleOptions_has_wrong_size);

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
    uint64_t num_bytes,  // In.
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

#endif  // MOJO_PUBLIC_C_SYSTEM_CORE_H_
