// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"
#include "ipc/ipc_message_macros.h"

IPC_BEGIN_MESSAGES(CommandBuffer)
  // Initialize a command buffer with the given number of command entries.
  // Returns the shared memory handle for the command buffer mapped to the
  // calling process.
  IPC_SYNC_MESSAGE_ROUTED1_1(CommandBufferMsg_Initialize,
                             int32 /* size */,
                             base::SharedMemoryHandle /* ring_buffer */)

  // Get the number of command entries in the command buffer.
  IPC_SYNC_MESSAGE_ROUTED0_1(CommandBufferMsg_GetSize,
                             int32 /* size */)

  // Synchronize the put and get offsets of both processes. Caller passes its
  // current put offset. Current get offset is returned.
  IPC_SYNC_MESSAGE_ROUTED1_1(CommandBufferMsg_SyncOffsets,
                             int32 /* put_offset */,
                             int32 /* get_offset */)

  // Get the current get offset.
  IPC_SYNC_MESSAGE_ROUTED0_1(CommandBufferMsg_GetGetOffset,
                             int32 /* get_offset */)

  // Get the current put offset.
  IPC_SYNC_MESSAGE_ROUTED0_1(CommandBufferMsg_GetPutOffset,
                             int32 /* put_offset */)

  // Create a shared memory transfer buffer. Returns an id that can be used to
  // identify the transfer buffer from a comment.
  IPC_SYNC_MESSAGE_ROUTED1_1(CommandBufferMsg_CreateTransferBuffer,
                             int32 /* size */,
                             int32 /* id */)

  // Destroy a previously created transfer buffer.
  IPC_SYNC_MESSAGE_ROUTED1_0(CommandBufferMsg_DestroyTransferBuffer,
                             int32 /* id */)

  // Get the shared memory handle for a transfer buffer mapped to the callers
  // process.
  IPC_SYNC_MESSAGE_ROUTED1_2(CommandBufferMsg_GetTransferBuffer,
                             int32 /* id */,
                             base::SharedMemoryHandle /* transfer_buffer */,
                             size_t /* size */)

  // Get the most recently processed token. Used for implementing fences.
  IPC_SYNC_MESSAGE_ROUTED0_1(CommandBufferMsg_GetToken,
                             int32 /* token */)

  // Get the current parse error. Calling this resets the parse error if it is
  // recoverable.
  // TODO(apatrick): Switch to the parse_error::ParseError enum now that NPAPI
  //    no longer limits to restricted set of datatypes.
  IPC_SYNC_MESSAGE_ROUTED0_1(CommandBufferMsg_ResetParseError,
                             int32 /* parse_error */)

  // Get the current error status.
  IPC_SYNC_MESSAGE_ROUTED0_1(CommandBufferMsg_GetErrorStatus,
                             bool /* status */)

IPC_END_MESSAGES(CommandBuffer)
