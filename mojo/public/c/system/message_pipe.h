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

// |MojoMessageHandle|: Used to refer to message objects.

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

typedef uint32_t MojoReadMessageFlags;

#ifdef __cplusplus
const MojoReadMessageFlags MOJO_READ_MESSAGE_FLAG_NONE = 0;
#else
#define MOJO_READ_MESSAGE_FLAG_NONE ((MojoReadMessageFlags)0)
#endif

// |MojoGetSerializedMessageContentsFlags|: Used to specify different options
// |MojoGetSerializedMessageContents()|.
//   |MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE| - No flags; default mode.

typedef uint32_t MojoGetSerializedMessageContentsFlags;

#ifdef __cplusplus
const MojoGetSerializedMessageContentsFlags
    MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE = 0;
#else
#define MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE \
  ((MojoGetSerializedMessageContentsFlags)0)
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
  // |MojoDestroyMessage()| or serialized for transmission across a process
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

// Writes a message to the message pipe endpoint given by |message_pipe_handle|.
//
// Note that regardless of success or failure, |message| is freed by this call
// and therefore invalidated.
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., the message was enqueued).
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message_pipe_handle| or |message| is
//       invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed.
//       Note that closing an endpoint is not necessarily synchronous (e.g.,
//       across processes), so this function may succeed even if the other
//       endpoint has been closed (in which case the message would be dropped).
MOJO_SYSTEM_EXPORT MojoResult MojoWriteMessage(MojoHandle message_pipe_handle,
                                               MojoMessageHandle message,
                                               MojoWriteMessageFlags flags);

// Reads the next message from a message pipe and returns a message as an opaque
// message handle. The returned message must eventually be destroyed using
// |MojoDestroyMessage()|.
//
// Message payload and handles can be accessed using
// |MojoGetSerializedMessageContents()|.
//
// |message| must be non-null.
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., a message was actually read).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed
//       and there are no more messages to read.
//   |MOJO_RESULT_SHOULD_WAIT| if no message was available to be read.
MOJO_SYSTEM_EXPORT MojoResult MojoReadMessage(MojoHandle message_pipe_handle,
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

// Creates a new message object which may be sent over a message pipe via
// |MojoWriteMessage()|. Returns a handle to the new message object in
// |*message|.
//
// |context| is an arbitrary context value to associate with this message, and
// |thunks| is a set of functions which may be called on |context| to perform
// various operations. See |MojoMessageOperationThunks| for details.
//
// Typically a caller will use |context| as an opaque pointer to some heap
// object which is effectively owned by the newly created message once this
// returns. In this way, messages can be sent over a message pipe to a peer
// endpoint in the same process as the sender without ever performing a
// serialization step.
//
// If the message does need to cross a process boundary or is otherwise
// forced to serialize (see |MojoSerializeMessage()| below), it will be
// serialized by invoking the serialization helper functions from |thunks| on
// |context| to obtain a serialized representation of the message. Note that
// because the need to serialize may occur at any time and on any thread,
// functions in |thunks| must be safe to invoke accordingly.
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

// Destroys a message object created by |MojoCreateMessage()| or
// |MojoReadMessage()|.
//
// |message|: The message to destroy. Note that if a message has been written
//     to a message pipe using |MojoWriteMessage()|, it is neither necessary nor
//     valid to attempt to destroy it.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| was valid and has been freed.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| was not a valid message.
MOJO_SYSTEM_EXPORT MojoResult MojoDestroyMessage(MojoMessageHandle message);

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
// |message|: The message to report as bad.
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
