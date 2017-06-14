// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/constants and functions specific to message pipes.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_MESSAGE_PIPE_H_
#define MOJO_PUBLIC_C_SYSTEM_MESSAGE_PIPE_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// |MojoMessageHandle|: Used to refer to message objects created by
//     |MojoAllocMessage()| and transferred by |MojoWriteMessageNew()| or
//     |MojoReadMessageNew()|.

typedef uintptr_t MojoMessageHandle;

#ifdef __cplusplus
const MojoMessageHandle MOJO_MESSAGE_HANDLE_INVALID = 0;
#else
#define MOJO_MESSAGE_HANDLE_INVALID ((MojoMessageHandle)0)
#endif

// |MojoCreateMessagePipeOptions|: Used to specify creation parameters for a
// message pipe to |MojoCreateMessagePipe()|.
//   |uint32_t struct_size|: Set to the size of the
//       |MojoCreateMessagePipeOptions| struct. (Used to allow for future
//       extensions.)
//   |MojoCreateMessagePipeOptionsFlags flags|: Used to specify different modes
//       of operation.
//       |MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE|: No flags; default mode.

typedef uint32_t MojoCreateMessagePipeOptionsFlags;

#ifdef __cplusplus
const MojoCreateMessagePipeOptionsFlags
    MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE = 0;
#else
#define MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE \
  ((MojoCreateMessagePipeOptionsFlags)0)
#endif

MOJO_STATIC_ASSERT(MOJO_ALIGNOF(int64_t) == 8, "int64_t has weird alignment");
struct MOJO_ALIGNAS(8) MojoCreateMessagePipeOptions {
  uint32_t struct_size;
  MojoCreateMessagePipeOptionsFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(MojoCreateMessagePipeOptions) == 8,
                   "MojoCreateMessagePipeOptions has wrong size");

// |MojoWriteMessageFlags|: Used to specify different modes to
// |MojoWriteMessage()|.
//   |MOJO_WRITE_MESSAGE_FLAG_NONE| - No flags; default mode.

typedef uint32_t MojoWriteMessageFlags;

#ifdef __cplusplus
const MojoWriteMessageFlags MOJO_WRITE_MESSAGE_FLAG_NONE = 0;
#else
#define MOJO_WRITE_MESSAGE_FLAG_NONE ((MojoWriteMessageFlags)0)
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
#define MOJO_READ_MESSAGE_FLAG_NONE ((MojoReadMessageFlags)0)
#define MOJO_READ_MESSAGE_FLAG_MAY_DISCARD ((MojoReadMessageFlags)1 << 0)
#endif

// |MojoAllocMessageFlags|: Used to specify different options for
// |MojoAllocMessage()|.
//   |MOJO_ALLOC_MESSAGE_FLAG_NONE| - No flags; default mode.

typedef uint32_t MojoAllocMessageFlags;

#ifdef __cplusplus
const MojoAllocMessageFlags MOJO_ALLOC_MESSAGE_FLAG_NONE = 0;
#else
#define MOJO_ALLOC_MESSAGE_FLAG_NONE ((MojoAllocMessageFlags)0)
#endif

// |MojoGetSerializedMessageContentsFlags|: Used to specify different options
// |MojoGetSerializedMessageContents()|.
//   |MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE| - No flags; default mode.
//   |MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_IGNORE_HANDLES| - Don't attempt
//       to extract any handles from the serialized message.

typedef uint32_t MojoGetSerializedMessageContentsFlags;

#ifdef __cplusplus
const MojoGetSerializedMessageContentsFlags
    MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE = 0;
const MojoGetSerializedMessageContentsFlags
    MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_IGNORE_HANDLES = 1 << 0;
#else
#define MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE \
  ((MojoGetSerializedMessageContentsFlags)0)
#define MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_IGNORE_HANDLES \
  ((MojoGetSerializedMessageContentsFlags)1 << 0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// A structure used with MojoCreateMessage to inform the system about how to
// operate on an opaque message object given by |context|.
//
// See |MojoCreateMessage()| for more details.
#pragma pack(push, 8)
struct MojoMessageOperationThunks {
  // Must be set to sizeof(MojoMessageOperationThunks).
  size_t struct_size;

  // Computes the number of bytes of payload and the number of handles required
  // to serialize the message associated with |context|. If the message needs
  // to be serialized, this is the first function invoked by the system,
  // followed immediately by |serialize_handles()| or |serialize_payload()| for
  // the same |context|.
  //
  // The implementation must populate |*num_bytes| and |*num_handles|.
  void (*get_serialized_size)(uintptr_t context,
                              size_t* num_bytes,
                              size_t* num_handles);

  // Passes ownership of any MojoHandles carried by the message associated with
  // |context|. Called immediately after |get_serialized_size()| for the same
  // |context|, if and only if the returned |*num_handles| was non-zero.
  //
  // The implementation must populate |handles| with a contiguous MojoHandle
  // array of exactly |*num_handles| (from above) elements.
  void (*serialize_handles)(uintptr_t context, MojoHandle* handles);

  // Serializes the message payload bytes into a message buffer. This always
  // follows a call to |get_serialized_size()| (and possibly
  // |serialize_handles()|) for the same |context|, if and only if |*num_bytes|
  // (from above) is non-zero.
  //
  // The implementation must populate |buffer| with exactly |*num_bytes| bytes
  // of message content. The storage in |buffer| is NOT initialized otherwise,
  // so it is the caller's responsibility to ensure that ALL |*num_bytes| bytes
  // are written to.
  void (*serialize_payload)(uintptr_t context, void* buffer);

  // Destroys the message object associated with |context|. This is called when
  // the message associated with |context| is either explicitly freed by
  // |MojoFreeMessage()| or serialized for transmission across a process
  // boundary (after serialization is complete.)
  //
  // The implementation must use this to release any resources associated with
  // |context|.
  void (*destroy)(uintptr_t context);
};
#pragma pack(pop)

// Note: See the comment in functions.h about the meaning of the "optional"
// label for pointer parameters.

// Creates a message pipe, which is a bidirectional communication channel for
// framed data (i.e., messages). Messages can contain plain data and/or Mojo
// handles.
//
// |options| may be set to null for a message pipe with the default options.
//
// On success, |*message_pipe_handle0| and |*message_pipe_handle1| are set to
// handles for the two endpoints (ports) for the message pipe.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |*options| is invalid).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateMessagePipe(
    const struct MojoCreateMessagePipeOptions* options,  // Optional.
    MojoHandle* message_pipe_handle0,                    // Out.
    MojoHandle* message_pipe_handle1);                   // Out.

// Writes a message to the message pipe endpoint given by |message_pipe_handle|,
// with message data specified by |bytes| of size |num_bytes| and attached
// handles specified by |handles| of count |num_handles|, and options specified
// by |flags|. If there is no message data, |bytes| may be null, in which case
// |num_bytes| must be zero. If there are no attached handles, |handles| may be
// null, in which case |num_handles| must be zero.
//
// If handles are attached, the handles will no longer be valid (on success the
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
//       across processes), so this function may succeed even if the other
//       endpoint has been closed (in which case the message would be dropped).
//   |MOJO_RESULT_UNIMPLEMENTED| if an unsupported flag was set in |*options|.
//   |MOJO_RESULT_BUSY| if some handle to be sent is currently in use.
//
// DEPRECATED: Use |MojoWriteMessageNew()|.
MOJO_SYSTEM_EXPORT MojoResult
    MojoWriteMessage(MojoHandle message_pipe_handle,
                     const void* bytes,  // Optional.
                     uint32_t num_bytes,
                     const MojoHandle* handles,  // Optional.
                     uint32_t num_handles,
                     MojoWriteMessageFlags flags);

// Writes a message to the message pipe endpoint given by |message_pipe_handle|.
//
// |message|: A message object allocated by |MojoAllocMessage()| or
//     |MojoCreateMessage()|. Ownership of the message is passed into Mojo.
//
// Returns results corresponding to |MojoWriteMessage()| above.
MOJO_SYSTEM_EXPORT MojoResult
    MojoWriteMessageNew(MojoHandle message_pipe_handle,
                        MojoMessageHandle message,
                        MojoWriteMessageFlags);

// Reads the next message from a message pipe, or indicates the size of the
// message if it cannot fit in the provided buffers. The message will be read
// in its entirety or not at all; if it is not, it will remain enqueued unless
// the |MOJO_READ_MESSAGE_FLAG_MAY_DISCARD| flag was passed. At most one
// message will be consumed from the queue, and the return value will indicate
// whether a message was successfully read.
//
// |num_bytes| and |num_handles| are optional in/out parameters that on input
// must be set to the sizes of the |bytes| and |handles| arrays, and on output
// will be set to the actual number of bytes or handles contained in the
// message (even if the message was not retrieved due to being too large).
// Either |num_bytes| or |num_handles| may be null if the message is not
// expected to contain the corresponding type of data, but such a call would
// fail with |MOJO_RESULT_RESOURCE_EXHAUSTED| if the message in fact did
// contain that type of data.
//
// |bytes| and |handles| will receive the contents of the message, if it is
// retrieved. Either or both may be null, in which case the corresponding size
// parameter(s) must also be set to zero or passed as null.
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., a message was actually read).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if the message was too large to fit in the
//       provided buffer(s). The message will have been left in the queue or
//       discarded, depending on flags.
//   |MOJO_RESULT_SHOULD_WAIT| if no message was available to be read.
//   |MOJO_RESULT_NOT_FOUND| if a message was read from the pipe but could not
//       be extracted into the provided buffers because it was not already in a
//       serialized form.
//
// DEPRECATED: Use |MojoReadMessageNew()|.
MOJO_SYSTEM_EXPORT MojoResult
    MojoReadMessage(MojoHandle message_pipe_handle,
                    void* bytes,            // Optional out.
                    uint32_t* num_bytes,    // Optional in/out.
                    MojoHandle* handles,    // Optional out.
                    uint32_t* num_handles,  // Optional in/out.
                    MojoReadMessageFlags flags);

// Reads the next message from a message pipe and returns a message containing
// the message bytes. The returned message must eventually be freed using
// |MojoFreeMessage()|.
//
// Message payload and handles can be accessed using
// |MojoGetSerializedMessageContents()|.
//
// |message| must be non-null unless |MOJO_READ_MESSAGE_FLAG_MAY_DISCARD| is set
//     in |flags|.
//
// Return values correspond to the return values for |MojoReadMessage()| above.
// On success (MOJO_RESULT_OK), |*message| will contain a handle to a message
// object which may be passed to |MojoGetSerializedMessageContents()| or
// |MojoReleaseMessageContext()|. The caller owns the message object and is
// responsible for freeing it via |MojoFreeMessage()| or
// |MojoReleaseMessageContext()|.
MOJO_SYSTEM_EXPORT MojoResult MojoReadMessageNew(MojoHandle message_pipe_handle,
                                                 MojoMessageHandle* message,
                                                 MojoReadMessageFlags flags);

// Fuses two message pipe endpoints together. Given two pipes:
//
//     A <-> B    and    C <-> D
//
// Fusing handle B and handle C results in a single pipe:
//
//     A <-> D
//
// Handles B and C are ALWAYS closed. Any unread messages at C will eventually
// be delivered to A, and any unread messages at B will eventually be delivered
// to D.
//
// NOTE: A handle may only be fused if it is an open message pipe handle which
// has not been written to.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_FAILED_PRECONDITION| if both handles were valid message pipe
//       handles but could not be merged (e.g. one of them has been written to).
//   |MOJO_INVALID_ARGUMENT| if either handle is not a fusable message pipe
//       handle.
MOJO_SYSTEM_EXPORT MojoResult
    MojoFuseMessagePipes(MojoHandle handle0, MojoHandle handle1);

// Allocates a new message whose ownership may be passed to
// |MojoWriteMessageNew()|. Use |MojoGetSerializedMessageContents()| to retrieve
// the address of the mutable message payload.
//
// |num_bytes|: The size of the message payload in bytes.
// |handles|: An array of handles to transfer in the message. This takes
//     ownership of and invalidates all contained handles. Must be null if and
//     only if |num_handles| is 0.
// |num_handles|: The number of handles contained in |handles|.
// |flags|: Must be |MOJO_CREATE_MESSAGE_FLAG_NONE|.
// |message|: The address of a handle to be filled with the allocated message's
//     handle. Must be non-null.
//
// Returns:
//   |MOJO_RESULT_OK| if the message was successfully allocated. In this case
//       |*message| will be populated with a handle to an allocated message
//       with a buffer large enough to hold |num_bytes| contiguous bytes.
//   |MOJO_RESULT_INVALID_ARGUMENT| if one or more handles in |handles| was
//       invalid, or |handles| was null with a non-zero |num_handles|.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if allocation failed because either
//       |num_bytes| or |num_handles| exceeds an implementation-defined maximum.
//   |MOJO_RESULT_BUSY| if one or more handles in |handles| cannot be sent at
//       the time of this call.
//
// Only upon successful message allocation will all handles in |handles| be
// transferred into the message and invalidated.
//
// DEPRECATED: Use |MojoCreateMessage()|.
MOJO_SYSTEM_EXPORT MojoResult
MojoAllocMessage(uint32_t num_bytes,
                 const MojoHandle* handles,
                 uint32_t num_handles,
                 MojoAllocMessageFlags flags,
                 MojoMessageHandle* message);  // Out

// Creates a new message object which may be sent over a message pipe via
// |MojoWriteMessageNew()|. Returns a handle to the new message object in
// |*message|.
//
// |context| is an arbitrary context value to associate with this message, and
// |thunks| is a set of functions which may be called on |context| to perform
// various operations. See |MojoMessageOperationThunks| for details.
//
// Unlike |MojoAllocMessage()| above, this does not assume that the message
// object will require serialization. Typically a caller will use |context| as
// an opaque pointer to some arbitrary heap object which is effectively owned by
// the newly created message once this returns. In this way, messages can be
// sent over a message pipe to a peer endpoint in the same process as the sender
// without ever performing a serialization step.
//
// If the message does need to cross a process boundary to reach its
// destination, it will be serialized lazily when necessary. To accomplish this,
// the system will invoke the serialization helper functions from |thunks| on
// |context| to obtain a serialized representation of the message.
//
// If |thunks| is null, the created message cannot be serialized. Subsequent
// calls to |MojoSerializeMessage()| on the created message, or any attempt to
// transmit the message across a process boundary, will fail.
//
// Returns:
//   |MOJO_RESULT_OK| if a new message was created. |*message| contains a handle
//       to the new message object on return.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |context| is 0, or |thunks| is non-null
//       and any element of |thunks| is invalid. In this case, the value of
//       |message| is ignored.
MOJO_SYSTEM_EXPORT MojoResult
MojoCreateMessage(uintptr_t context,
                  const struct MojoMessageOperationThunks* thunks,
                  MojoMessageHandle* message);

// Frees a message allocated by |MojoAllocMessage()| or |MojoReadMessageNew()|.
//
// |message|: The message to free. This must correspond to a message previously
//     allocated by |MojoAllocMessage()| or |MojoReadMessageNew()|. Note that if
//     the message has already been passed to |MojoWriteMessageNew()| it should
//     NOT also be freed with this API.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| was valid and has been freed.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| was not a valid message.
MOJO_SYSTEM_EXPORT MojoResult MojoFreeMessage(MojoMessageHandle message);

// Forces a message to be serialized in-place if not already serialized.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| was not serialized and is now serialized.
//       In this case its thunks were invoked to perform serialization and
//       ultimately destroy its associated context. The message may still be
//       written to a pipe or decomposed by MojoGetSerializedMessageContents().
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| was already serialized.
//   |MOJO_RESULT_NOT_FOUND| if |message| cannot be serialized (i.e. it was
//       created with null |MojoMessageOperationThunks|.)
//   |MOJO_RESULT_BUSY| if one or more handles provided by the user context
//       reported itself as busy during the serialization attempt. In this case
//       all serialized handles are closed automatically.
//   |MOJO_RESULT_ABORTED| if some other unspecified error occurred during
//       handle serialization. In this case all serialized handles are closed
//       automatically.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message handle.
//
// Note that unserialized messages may be successfully transferred from one
// message pipe endpoint to another without ever being serialized. This function
// allows callers to coerce eager serialization.
MOJO_SYSTEM_EXPORT MojoResult MojoSerializeMessage(MojoMessageHandle message);

// Retrieves the contents of a serialized message.
//
// |message|: The message whose contents are to be retrieved.
// |num_bytes|: An output parameter which will receive the total size in bytes
//     of the message's payload.
// |buffer|: An output parameter which will receive the address of a buffer
//     containing exactly |*num_bytes| bytes of payload data. This buffer
//     address is not owned by the caller and is only valid as long as the
//     message handle in |message| is valid.
// |num_handles|: An input/output parameter. On input, if not null, this points
//     to value specifying the available capacity (in number of handles) of
//     |handles|. On output, if not null, this will point to a value specifying
//     the actual number of handles available in the serialized message.
// |handles|: A buffer to contain up to (input) |*num_handles| handles. May be
//     null if |num_handles| is null or |*num_handles| is 0.
// |flags|: Flags to affect the behavior of this API.
//
// If |MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_IGNORE_HANDLES| is set in
// |flags|, |num_handles| and |handles| arguments are ignored and only payload-
// related outputs are updated.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| is a serialized message and the provided
//       handle storage is sufficient to contain all handles attached to the
//       message. In this case all non-null output parameters are filled in and
//       ownership of any attached handles is transferred to the caller. It is
//       no longer legal to call MojoGetSerializedMessageContents on this
//       message handle.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |num_handles| is non-null and
//       |*num_handles| is non-zero, but |handles| is null; or if |message| is
//       not a valid message handle.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| is not a serialized message.
//       The caller may either use |MojoSerializeMessage()| and try again, or
//       use |MojoReleaseMessageContext()| to extract the message's unserialized
//       context.
//   |MOJO_RESULT_NOT_FOUND| if the message's serialized contents have already
//       been extracted (or have failed to be extracted) by a previous call to
//       |MojoGetSerializedMessageContents()|.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if |num_handles| is null and there are
//       handles attached to the message, or if |*num_handles| on input is less
//       than the number of handles attached to the message. Also may be
//       returned if |num_bytes| or |buffer| is null and the message has a non-
//       empty payload.
//   |MOJO_RESULT_ARBORTED| if the serialized message could not be parsed or
//       its attached handles could not be decoded properly. The message is left
//       intact but is effectively useless: future calls to this API on the same
//       message handle will yield the same result.
MOJO_SYSTEM_EXPORT MojoResult
MojoGetSerializedMessageContents(MojoMessageHandle message,
                                 void** buffer,
                                 uint32_t* num_bytes,
                                 MojoHandle* handles,
                                 uint32_t* num_handles,
                                 MojoGetSerializedMessageContentsFlags flags);

// Detaches the user-provided context from a message and returns it to the
// caller. This can only succeed if the message is not in a serialized form.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| was a valid message object which has not yet
//       been serialized. Upon return, |*context| contains the context value
//       originally provided to the |MojoCreateMessage()| call which created
//       |message|.
//   |MOJO_RESULT_NOT_FOUND| if |message| is a valid message object which has no
//       associated context value. In this case it must be a serialized message,
//       and |MojoGetSerializedMessageContents()| should be called instead.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object.
MOJO_SYSTEM_EXPORT MojoResult
MojoReleaseMessageContext(MojoMessageHandle message, uintptr_t* context);

// Notifies the system that a bad message was received on a message pipe,
// according to whatever criteria the caller chooses. This ultimately tries to
// notify the embedder about the bad message, and the embedder may enforce some
// policy for dealing with the source of the message (e.g. close the pipe,
// terminate, a process, etc.) The embedder may not be notified if the calling
// process has lost its connection to the source process.
//
// |message|: The message to report as bad. This must have come from a call to
//     |MojoReadMessageNew()|.
// |error|: An error string which may provide the embedder with context when
//     notified of this error.
// |error_num_bytes|: The length of |error| in bytes.
//
// Returns:
//   |MOJO_RESULT_OK| if successful.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message.
MOJO_SYSTEM_EXPORT MojoResult
MojoNotifyBadMessage(MojoMessageHandle message,
                     const char* error,
                     size_t error_num_bytes);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_MESSAGE_PIPE_H_
