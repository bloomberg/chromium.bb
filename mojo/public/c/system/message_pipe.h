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

// |MojoGetMessageContextFlags|: Used to specify different options for
// |MojoGetMessageContext|.
//   |MOJO_GET_MESSAGE_CONTEXT_FLAG_NONE| - No flags; default mode.
//   |MOJO_GET_MESSAGE_CONTEXT_FLAG_RELEASE| - Causes the message object to
//       release its reference to its own context before returning.

typedef uint32_t MojoGetMessageContextFlags;

#ifdef __cplusplus
const MojoGetMessageContextFlags MOJO_GET_MESSAGE_CONTEXT_FLAG_NONE = 0;
const MojoGetMessageContextFlags MOJO_GET_MESSAGE_CONTEXT_FLAG_RELEASE = 1;
#else
#define MOJO_GET_MESSAGE_CONTEXT_FLAG_NONE ((MojoGetMessageContextFlags)0)
#define MOJO_GET_MESSAGE_CONTEXT_FLAG_RELEASE ((MojoGetMessageContextFlags)1)
#endif

// |MojoGetSerializedMessageContentsFlags|: Used to specify different options
// for |MojoGetSerializedMessageContents()|.
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

// A callback which can serialize a message given some context. Passed to
// MojoAttachMessageContext along with a context it knows how to serialize.
// See |MojoAttachMessageContext()| for more details.
//
//   |message| is a message object which had |context| attached.
//   |context| the context which was attached to |message|.
//
// Note that the context is always detached from the message immediately before
// this callback is invoked, and that the associated destructor (if any) is also
// invoked on |context| immediately after the serializer returns.
typedef void (*MojoMessageContextSerializer)(MojoMessageHandle message,
                                             uintptr_t context);

// A callback which can be used to destroy a message context after serialization
// or in the event that the message to which it's attached is destroyed without
// ever being serialized.
typedef void (*MojoMessageContextDestructor)(uintptr_t context);

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
// Note that regardless of success or failure, |message| is destroyed by this
// call and therefore invalidated.
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., the message was enqueued).
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message_pipe_handle| or |message| is
//       invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed.
//       Note that closing an endpoint is not necessarily synchronous (e.g.,
//       across processes), so this function may succeed even if the other
//       endpoint has been closed (in which case the message would be dropped).
//   |MOJO_RESULT_NOT_FOUND| if |message| has neither a context nor serialized
//       buffer attached and therefore has nothing to be written.
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
// In its initial state the message object cannot be successfully written to a
// message pipe, but must first have either a context or serialized buffer
// attached (see |MojoAttachContext()| and |MojoAttachSerializedMessageBuffer()|
// below.)
//
// NOTE: Unlike |MojoHandle|, a |MojoMessageHandle| is NOT thread-safe and thus
// callers of message-related APIs must be careful to restrict usage of any
// given |MojoMessageHandle| to a single thread at a time.
//
// Returns:
//   |MOJO_RESULT_OK| if a new message was created. |*message| contains a handle
//       to the new message object upon return.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is null.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateMessage(MojoMessageHandle* message);

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
//       created with null |MojoMessageContextSerializer|.)
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

// Attaches a serialized message buffer to a message object.
//
// |message|: The message to which a buffer is to be attached.
// |payload_size|: The initial expected payload size of the message. If this
//     call succeeds, the attached buffer will be at least this large.
// |handles|: The handles to attach to the serialized message. The set of
//     attached handles may be augmented by one or more calls to
//     |MojoExtendSerializedMessagePayload|. May be null iff |num_handles| is 0.
// |num_handles|: The number of handles provided by |handles|.
//
// Note that while a serialized message buffer's size may exceed the size of the
// payload, when a serialized message is transmitted (or its contents retrieved
// with |MojoGetSerializedMessageContents()|), only the extent of the payload
// is transmitted and/or exposed. Use |MojoExtendSerializedMessagePayload()| to
// extend the portion of the buffer which is designated as valid payload and
// (if necessary) expand the available capacity.
//
// It is legal to write past |payload_size| (up to |*buffer_size|) within
// |*buffer| upon return, but a future call to
// |MojoExtendSerializedMessagePayload()| or
// |MojoCommitSerializedMessageContents()| must be issued to account for the new
// desired payload boundary and ensure that those bytes are transmitted in the
// message.
//
// Ownership of all handles in |handles| is transferred to the message object if
// and ONLY if this operation succeeds and returns |MOJO_RESULT_OK|. Otherwise
// the caller retains ownership.
//
// Returns:
//   |MOJO_RESULT_OK| upon success. A new serialized message buffer has been
//       attached to |message|. The address of the buffer's storage is output in
//       |*buffer| and its size is in |*buffer_size|. The message is considered
//       to be in a partially serialized state and is not transmittable until
//       |MojoCommitSerializedMessageContents()| is called.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object;
//       if |num_handles| is non-zero but |handles| is null; if either
//       |buffer| or |buffer_size| is null; or if any handle in |handles| is
//       invalid.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if |payload_size| or |num_handles| exceeds
//       some implementation- or embedder-defined maximum.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| has a context attached.
//   |MOJO_RESULT_ALREADY_EXISTS| if |message| already has a serialized buffer
//       attached.
//   |MOJO_RESULT_BUSY| if one or more handles in |handles| is currently busy
//       and unable to be serialized.
MOJO_SYSTEM_EXPORT MojoResult
MojoAttachSerializedMessageBuffer(MojoMessageHandle message,
                                  uint32_t payload_size,
                                  const MojoHandle* handles,
                                  uint32_t num_handles,
                                  void** buffer,
                                  uint32_t* buffer_size);

// Extends the contents of a partially serialized message with additional
// payload bytes and/or handle attachments. If the underlying message buffer is
// not large enough to accomodate the additional payload size, this will attempt
// to expand the buffer before returning.
//
// May only be called on partially serialized messages, i.e. between a call
// to |MojoAttachSerializedMessageBuffer()| and
// |MojoCommitSerializedMessageContents()|.
//
// |message|: The message getting additional payload.
// |new_payload_size|: The new total of the payload. Must be at least as large
//     as the message's current payload size.
// |handles|: Handles to be added to the serialized message. These are amended
//     to the message's current set of serialized handles, if any. May be null
//     iff |num_handles| is 0.
// |num_handles|: The number of handles in |handles|.
//
// As with |MojoAttachSerializedMessageBuffer|, it is legal to write past
// |payload_size| (up to |*buffer_size|) within |*buffer| upon return, but a
// future call to |MojoExtendSerializedMessagePayload()| or
// |MojoCommitSerializedMessageContents()| must be issued to account for the new
// desired payload boundary and ensure that those bytes are transmitted in the
// message.
//
// Ownership of all handles in |handles| is transferred to the message object if
// and ONLY if this operation succeeds and returns |MOJO_RESULT_OK|. Otherwise
// the caller retains ownership.
//
// Returns:
//   |MOJO_RESULT_OK| if the new payload size has been committed to the message.
//       If necessary, the message's buffer may have been reallocated to
//       accommodate additional capacity. |*buffer| and |*buffer_size| contain
//       the (possibly changed) address and size of the serialized message
//       buffer's storage.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object,
//       or either |buffer| or |buffer_size| is null.
//   |MOJO_RESULT_OUT_OF_RANGE| if |payload_size| is not at least as large as
//       the message's current payload size.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| does not have a serialized
//       message buffer attached or is already fully serialized (i.e.
//       |MojoCommitSerializedMessageContents()| has been called).
//   |MOJO_RESULT_BUSY| if one or more handles in |handles| is currently busy
//       and unable to be serialized.
MOJO_SYSTEM_EXPORT MojoResult
MojoExtendSerializedMessagePayload(MojoMessageHandle message,
                                   uint32_t new_payload_size,
                                   const MojoHandle* handles,
                                   uint32_t num_handles,
                                   void** new_buffer,
                                   uint32_t* new_buffer_size);

// Prepares a partially serialized message for transmission. MUST be called on
// a partially serialized message before it is legal to either write the message
// to a message pipe or call |MojoGetSerializedMessageContents()|.
//
// |message|: The message whose contents are to be committed.
// |final_payload_size|: The total number of bytes of meaningful payload to
//     treat as the full message body.
//
// Note that upon return, because no further calls to
// |MojoExtendSerializedMessagePayload()| are allowed on |message|, the payload
// may not be extended further and no more handles may be attached.
//
// The message's payload buffer (returned and potentially relocated by this
// call) however is still valid and mutable until the message is either
// destroyed or written to a pipe.
//
// Returns:
//   |MOJO_RESULT_OK| if the message contents are committed. The message is
//       fully serialized and it is now legal to write it to a pipe or extract
//       its contents via |MojoGetSerializedMessageContents()|. |*buffer| and
//       |*buffer_size| on output will contain the address and size of the
//       payload buffer, which may be relocated one final time by this call.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object.
//   |MOJO_RESULT_OUT_OF_RANGE| if |final_payload_size| is larger than the
//       message's available buffer size as indicated by the most recent call
//       to |MojoAttachSerializedMessageBuffer()| or
//       |MojoExtendSerializedMessagePayload()|, or smaller than the most
//       recently requested payload size given to either call.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| is not a (partially or
//       fully) serialized message object.
MOJO_SYSTEM_EXPORT MojoResult
MojoCommitSerializedMessageContents(MojoMessageHandle message,
                                    uint32_t final_payload_size,
                                    void** buffer,
                                    uint32_t* buffer_size);

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
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| is not a fully serialized
//       message. The caller may either use |MojoSerializeMessage()| and try
//       again, |MojoCommitSerializedMessageContents()| to complete a partially
//       serialized |message|, or |MojoGetMessageContext()| to extract
//       |message|'s unserialized context, depending on the message's state.
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

// Attachs an unserialized message context to a message.
//
// |context| is the context value to associate with this message, and
// |serializer| is a function which may be called at some later time to convert
// |message| to a serialized message object using |context| as input. See
// |MojoMessageContextSerializer| for more details. |destructor| is a function
// which may be called to allow the user to cleanup state associated with
// |context| after serialization or in the event that the message is destroyed
// without ever being serialized.
//
// Typically a caller will use |context| as an opaque pointer to some heap
// object which is effectively owned by the newly created message once this
// returns. In this way, messages can be sent over a message pipe to a peer
// endpoint in the same process as the sender without ever performing a
// serialization step.
//
// If the message does need to cross a process boundary or is otherwise
// forced to serialize (see |MojoSerializeMessage()| below), it will be
// serialized by invoking |serializer|.
//
// If |serializer| is null, the created message cannot be serialized. Subsequent
// calls to |MojoSerializeMessage()| on the created message, or any attempt to
// transmit the message across a process boundary, will fail.
//
// If |destructor| is null, it is assumed that no cleanup is required after
// serializing or destroying a message with |context| attached.
//
// Returns:
//   |MOJO_RESULT_OK| if the context was successfully attached.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |context| is 0 or |message| is not a
//       valid message object.
//   |MOJO_RESULT_ALREADY_EXISTS| if |message| already has a context attached.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| already has a serialized
//       buffer attached.
MOJO_SYSTEM_EXPORT MojoResult
MojoAttachMessageContext(MojoMessageHandle message,
                         uintptr_t context,
                         MojoMessageContextSerializer serializer,
                         MojoMessageContextDestructor destructor);

// Extracts the user-provided context from a message and returns it to the
// caller.
//
// |flags|: Flags to alter the behavior of this call. See
//     |MojoGetMessageContextFlags| for details.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| is a valid message object which has a
//       |context| attached. Upon return, |*context| contains the value of the
//       attached context. If |flags| contains
//       |MOJO_GET_MESSAGE_CONTEXT_FLAG_RELEASE|, the |context| is detached from
//       |message|.
//   |MOJO_RESULT_NOT_FOUND| if |message| is a valid message object which has no
//       attached context.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object.
MOJO_SYSTEM_EXPORT MojoResult
MojoGetMessageContext(MojoMessageHandle message,
                      uintptr_t* context,
                      MojoGetMessageContextFlags flags);

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
