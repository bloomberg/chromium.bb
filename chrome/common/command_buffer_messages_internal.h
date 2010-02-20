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

  // Get the current state of the command buffer.
  IPC_SYNC_MESSAGE_ROUTED0_1(CommandBufferMsg_GetState,
                             gpu::CommandBuffer::State /* state */)

  // Get the current state of the command buffer asynchronously. State is
  // returned via UpdateState message.
  IPC_MESSAGE_ROUTED0(CommandBufferMsg_AsyncGetState)

  // Synchronize the put and get offsets of both processes. Caller passes its
  // current put offset. Current state (including get offset) is returned.
  IPC_SYNC_MESSAGE_ROUTED1_1(CommandBufferMsg_Flush,
                             int32 /* put_offset */,
                             gpu::CommandBuffer::State /* state */)

  // Asynchronously synchronize the put and get offsets of both processes.
  // Caller passes its current put offset. Current state (including get offset)
  // is returned via an UpdateState message.
  IPC_MESSAGE_ROUTED1(CommandBufferMsg_AsyncFlush,
                      int32 /* put_offset */)

  // Return the current state of the command buffer following a request via
  // an AsyncGetState or AsyncFlush message.
  IPC_MESSAGE_ROUTED1(CommandBufferMsg_UpdateState,
                      gpu::CommandBuffer::State /* state */)

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
                             uint32 /* size */)

#if defined(OS_MACOSX)
  // On Mac OS X the GPU plugin must be offscreen, because there is no
  // true cross-process window hierarchy. For this reason we must send
  // resize events explicitly to the command buffer stub so it can
  // reallocate its backing store and send the new one back to the
  // browser. This message is currently used only on 10.6 and later.
  IPC_MESSAGE_ROUTED2(CommandBufferMsg_SetWindowSize,
                      int32 /* width */,
                      int32 /* height */)
#endif

IPC_END_MESSAGES(CommandBuffer)
