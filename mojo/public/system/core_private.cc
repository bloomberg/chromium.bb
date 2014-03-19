// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/system/core_private.h"

#include <assert.h>
#include <stddef.h>

static mojo::Core* g_core = NULL;

extern "C" {

MojoTimeTicks MojoGetTimeTicksNow() {
  assert(g_core);
  return g_core->GetTimeTicksNow();
}

MojoResult MojoClose(MojoHandle handle) {
  assert(g_core);
  return g_core->Close(handle);
}

MojoResult MojoWait(MojoHandle handle,
                    MojoWaitFlags flags,
                    MojoDeadline deadline) {
  assert(g_core);
  return g_core->Wait(handle, flags, deadline);
}

MojoResult MojoWaitMany(const MojoHandle* handles,
                        const MojoWaitFlags* flags,
                        uint32_t num_handles,
                        MojoDeadline deadline) {
  assert(g_core);
  return g_core->WaitMany(handles, flags, num_handles, deadline);
}

MojoResult MojoCreateMessagePipe(MojoHandle* message_pipe_handle0,
                                 MojoHandle* message_pipe_handle1) {
  assert(g_core);
  return g_core->CreateMessagePipe(message_pipe_handle0, message_pipe_handle1);
}

MojoResult MojoWriteMessage(MojoHandle message_pipe_handle,
                            const void* bytes,
                            uint32_t num_bytes,
                            const MojoHandle* handles,
                            uint32_t num_handles,
                            MojoWriteMessageFlags flags) {
  assert(g_core);
  return g_core->WriteMessage(message_pipe_handle, bytes, num_bytes, handles,
                              num_handles, flags);
}

MojoResult MojoReadMessage(MojoHandle message_pipe_handle,
                           void* bytes,
                           uint32_t* num_bytes,
                           MojoHandle* handles,
                           uint32_t* num_handles,
                           MojoReadMessageFlags flags) {
  assert(g_core);
  return g_core->ReadMessage(message_pipe_handle, bytes, num_bytes, handles,
                             num_handles, flags);
}

MojoResult MojoCreateDataPipe(const MojoCreateDataPipeOptions* options,
                              MojoHandle* data_pipe_producer_handle,
                              MojoHandle* data_pipe_consumer_handle) {
  assert(g_core);
  return g_core->CreateDataPipe(options, data_pipe_producer_handle,
                                data_pipe_consumer_handle);
}

MojoResult MojoWriteData(MojoHandle data_pipe_producer_handle,
                         const void* elements,
                         uint32_t* num_elements,
                         MojoWriteDataFlags flags) {
  assert(g_core);
  return g_core->WriteData(data_pipe_producer_handle, elements, num_elements,
                           flags);
}

MojoResult MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                              void** buffer,
                              uint32_t* buffer_num_elements,
                              MojoWriteDataFlags flags) {
  assert(g_core);
  return g_core->BeginWriteData(data_pipe_producer_handle, buffer,
                                buffer_num_elements, flags);
}

MojoResult MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                            uint32_t num_elements_written) {
  assert(g_core);
  return g_core->EndWriteData(data_pipe_producer_handle, num_elements_written);
}

MojoResult MojoReadData(MojoHandle data_pipe_consumer_handle,
                        void* elements,
                        uint32_t* num_elements,
                        MojoReadDataFlags flags) {
  assert(g_core);
  return g_core->ReadData(data_pipe_consumer_handle, elements, num_elements,
                          flags);
}

MojoResult MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                             const void** buffer,
                             uint32_t* buffer_num_elements,
                             MojoReadDataFlags flags) {
  assert(g_core);
  return g_core->BeginReadData(data_pipe_consumer_handle, buffer,
                               buffer_num_elements, flags);
}

MojoResult MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                           uint32_t num_elements_read) {
  assert(g_core);
  return g_core->EndReadData(data_pipe_consumer_handle, num_elements_read);
}

MojoResult MojoCreateSharedBuffer(
    const struct MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  assert(g_core);
  return g_core->CreateSharedBuffer(options, num_bytes, shared_buffer_handle);
}

MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  assert(g_core);
  return g_core->DuplicateBufferHandle(buffer_handle, options,
                                       new_buffer_handle);
}

MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                         uint64_t offset,
                         uint64_t num_bytes,
                         void** buffer,
                         MojoMapBufferFlags flags) {
  assert(g_core);
  return g_core->MapBuffer(buffer_handle, offset, num_bytes, buffer, flags);
}

MojoResult MojoUnmapBuffer(void* buffer) {
  assert(g_core);
  return g_core->UnmapBuffer(buffer);
}

}  // extern "C"

namespace mojo {

Core::~Core() {
}

// static
void Core::Init(Core* core) {
  assert(!g_core);
  g_core = core;
}

// static
Core* Core::Get() {
  return g_core;
}

// static
void Core::Reset() {
  g_core = NULL;
}

}  // namespace mojo
