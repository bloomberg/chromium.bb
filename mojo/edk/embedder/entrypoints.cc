// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/wait_set.h"

using mojo::edk::internal::g_core;

// Definitions of the system functions.
extern "C" {

MojoTimeTicks MojoGetTimeTicksNow() {
  return g_core->GetTimeTicksNow();
}

MojoResult MojoClose(MojoHandle handle) {
  return g_core->Close(handle);
}

MojoResult MojoWait(MojoHandle handle,
                    MojoHandleSignals signals,
                    MojoDeadline deadline,
                    MojoHandleSignalsState* signals_state) {
  return g_core->Wait(handle, signals, deadline, signals_state);
}

MojoResult MojoWaitMany(const MojoHandle* handles,
                        const MojoHandleSignals* signals,
                        uint32_t num_handles,
                        MojoDeadline deadline,
                        uint32_t* result_index,
                        MojoHandleSignalsState* signals_states) {
  return g_core->WaitMany(handles, signals, num_handles, deadline, result_index,
                          signals_states);
}

MojoResult MojoWatch(MojoHandle handle,
                     MojoHandleSignals signals,
                     MojoWatchCallback callback,
                     uintptr_t context) {
  return g_core->Watch(handle, signals, callback, context);
}

MojoResult MojoCancelWatch(MojoHandle handle, uintptr_t context) {
  return g_core->CancelWatch(handle, context);
}

MojoResult MojoAllocMessage(uint32_t num_bytes,
                            const MojoHandle* handles,
                            uint32_t num_handles,
                            MojoAllocMessageFlags flags,
                            MojoMessageHandle* message) {
  return g_core->AllocMessage(num_bytes, handles, num_handles, flags, message);
}

MojoResult MojoFreeMessage(MojoMessageHandle message) {
  return g_core->FreeMessage(message);
}

MojoResult MojoGetMessageBuffer(MojoMessageHandle message, void** buffer) {
  return g_core->GetMessageBuffer(message, buffer);
}

MojoResult MojoCreateWaitSet(MojoHandle* wait_set_handle) {
  return g_core->CreateWaitSet(wait_set_handle);
}

MojoResult MojoAddHandle(MojoHandle wait_set_handle,
                         MojoHandle handle,
                         MojoHandleSignals signals) {
  return g_core->AddHandle(wait_set_handle, handle, signals);
}

MojoResult MojoRemoveHandle(MojoHandle wait_set_handle, MojoHandle handle) {
  return g_core->RemoveHandle(wait_set_handle, handle);
}

MojoResult MojoGetReadyHandles(MojoHandle wait_set_handle,
                               uint32_t* count,
                               MojoHandle* handles,
                               MojoResult* results,
                               struct MojoHandleSignalsState* signals_states) {
  return g_core->GetReadyHandles(wait_set_handle, count, handles, results,
                                 signals_states);
}

MojoResult MojoCreateMessagePipe(const MojoCreateMessagePipeOptions* options,
                                 MojoHandle* message_pipe_handle0,
                                 MojoHandle* message_pipe_handle1) {
  return g_core->CreateMessagePipe(options, message_pipe_handle0,
                                   message_pipe_handle1);
}

MojoResult MojoWriteMessage(MojoHandle message_pipe_handle,
                            const void* bytes,
                            uint32_t num_bytes,
                            const MojoHandle* handles,
                            uint32_t num_handles,
                            MojoWriteMessageFlags flags) {
  return g_core->WriteMessage(message_pipe_handle, bytes, num_bytes, handles,
                              num_handles, flags);
}

MojoResult MojoWriteMessageNew(MojoHandle message_pipe_handle,
                               MojoMessageHandle message,
                               MojoWriteMessageFlags flags) {
  return g_core->WriteMessageNew(message_pipe_handle, message, flags);
}

MojoResult MojoReadMessage(MojoHandle message_pipe_handle,
                           void* bytes,
                           uint32_t* num_bytes,
                           MojoHandle* handles,
                           uint32_t* num_handles,
                           MojoReadMessageFlags flags) {
  return g_core->ReadMessage(
      message_pipe_handle, bytes, num_bytes, handles, num_handles, flags);
}

MojoResult MojoReadMessageNew(MojoHandle message_pipe_handle,
                              MojoMessageHandle* message,
                              uint32_t* num_bytes,
                              MojoHandle* handles,
                              uint32_t* num_handles,
                              MojoReadMessageFlags flags) {
  return g_core->ReadMessageNew(
      message_pipe_handle, message, num_bytes, handles, num_handles, flags);
}

MojoResult MojoFuseMessagePipes(MojoHandle handle0, MojoHandle handle1) {
  return g_core->FuseMessagePipes(handle0, handle1);
}

MojoResult MojoCreateDataPipe(const MojoCreateDataPipeOptions* options,
                              MojoHandle* data_pipe_producer_handle,
                              MojoHandle* data_pipe_consumer_handle) {
  return g_core->CreateDataPipe(options, data_pipe_producer_handle,
                                data_pipe_consumer_handle);
}

MojoResult MojoWriteData(MojoHandle data_pipe_producer_handle,
                         const void* elements,
                         uint32_t* num_elements,
                         MojoWriteDataFlags flags) {
  return g_core->WriteData(data_pipe_producer_handle, elements, num_elements,
                           flags);
}

MojoResult MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                              void** buffer,
                              uint32_t* buffer_num_elements,
                              MojoWriteDataFlags flags) {
  return g_core->BeginWriteData(data_pipe_producer_handle, buffer,
                                buffer_num_elements, flags);
}

MojoResult MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                            uint32_t num_elements_written) {
  return g_core->EndWriteData(data_pipe_producer_handle, num_elements_written);
}

MojoResult MojoReadData(MojoHandle data_pipe_consumer_handle,
                        void* elements,
                        uint32_t* num_elements,
                        MojoReadDataFlags flags) {
  return g_core->ReadData(data_pipe_consumer_handle, elements, num_elements,
                          flags);
}

MojoResult MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                             const void** buffer,
                             uint32_t* buffer_num_elements,
                             MojoReadDataFlags flags) {
  return g_core->BeginReadData(data_pipe_consumer_handle, buffer,
                               buffer_num_elements, flags);
}

MojoResult MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                           uint32_t num_elements_read) {
  return g_core->EndReadData(data_pipe_consumer_handle, num_elements_read);
}

MojoResult MojoCreateSharedBuffer(
    const struct MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  return g_core->CreateSharedBuffer(options, num_bytes, shared_buffer_handle);
}

MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  return g_core->DuplicateBufferHandle(buffer_handle, options,
                                       new_buffer_handle);
}

MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                         uint64_t offset,
                         uint64_t num_bytes,
                         void** buffer,
                         MojoMapBufferFlags flags) {
  return g_core->MapBuffer(buffer_handle, offset, num_bytes, buffer, flags);
}

MojoResult MojoUnmapBuffer(void* buffer) {
  return g_core->UnmapBuffer(buffer);
}

}  // extern "C"
