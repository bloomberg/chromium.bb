// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/platform/native/system_impl_private.h"

using mojo::edk::internal::g_core;
using mojo::edk::Core;
using mojo::edk::Dispatcher;

// TODO(use_chrome_edk): commented out since for now we use the entrypoints in
// third_party and that checks the command line to redirect here.
/*

// Definitions of the system functions, but with an explicit parameter for the
// core object rather than using the default singleton. Also includes functions
// for manipulating core objects.
extern "C" {

MojoSystemImpl MojoSystemImplGetDefaultImpl() {
  return static_cast<MojoSystemImpl>(g_core);
}

MojoSystemImpl MojoSystemImplCreateImpl() {
  Core* created_core = new Core(g_core->platform_support());
  return static_cast<MojoSystemImpl>(created_core);
}

MojoResult MojoSystemImplTransferHandle(MojoSystemImpl from_system,
                                        MojoHandle handle,
                                        MojoSystemImpl to_system,
                                        MojoHandle* result_handle) {
  Core* from_core = static_cast<Core*>(from_system);
  if (from_core == nullptr)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (handle == MOJO_HANDLE_INVALID)
    return MOJO_RESULT_INVALID_ARGUMENT;

  Core* to_core = static_cast<Core*>(to_system);
  if (to_core == nullptr)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (result_handle == nullptr)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> d;
  MojoResult result = from_core->GetAndRemoveDispatcher(handle, &d);
  if (result != MOJO_RESULT_OK)
    return result;

  MojoHandle created_handle = to_core->AddDispatcher(d);
  if (created_handle == MOJO_HANDLE_INVALID) {
    // The handle has been lost, unfortunately. There's no guarentee we can put
    // it back where it came from, or get the original ID back. Holding locks
    // for multiple cores risks deadlock, so that isn't a solution. This case
    // should not happen for reasonable uses of this API, however.
    LOG(ERROR) << "Could not transfer handle";
    d->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *result_handle = created_handle;
  return MOJO_RESULT_OK;
}

MojoTimeTicks MojoSystemImplGetTimeTicksNow(MojoSystemImpl system) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->GetTimeTicksNow();
}

MojoResult MojoSystemImplClose(MojoSystemImpl system, MojoHandle handle) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->Close(handle);
}

MojoResult MojoSystemImplWait(MojoSystemImpl system,
                              MojoHandle handle,
                              MojoHandleSignals signals,
                              MojoDeadline deadline,
                              MojoHandleSignalsState* signals_state) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->Wait(handle, signals, deadline, signals_state);
}

MojoResult MojoSystemImplWaitMany(MojoSystemImpl system,
                                  const MojoHandle* handles,
                                  const MojoHandleSignals* signals,
                                  uint32_t num_handles,
                                  MojoDeadline deadline,
                                  uint32_t* result_index,
                                  MojoHandleSignalsState* signals_states) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->WaitMany(handles, signals, num_handles, deadline, result_index,
                        signals_states);
}

MojoResult MojoSystemImplCreateMessagePipe(
    MojoSystemImpl system,
    const MojoCreateMessagePipeOptions* options,
    MojoHandle* message_pipe_handle0,
    MojoHandle* message_pipe_handle1) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->CreateMessagePipe(options, message_pipe_handle0,
                                 message_pipe_handle1);
}

MojoResult MojoSystemImplWriteMessage(MojoSystemImpl system,
                                      MojoHandle message_pipe_handle,
                                      const void* bytes,
                                      uint32_t num_bytes,
                                      const MojoHandle* handles,
                                      uint32_t num_handles,
                                      MojoWriteMessageFlags flags) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->WriteMessage(message_pipe_handle, bytes, num_bytes, num_bytes,
                            handles, num_handles, flags);
}

MojoResult MojoSystemImplReadMessage(MojoSystemImpl system,
                                     MojoHandle message_pipe_handle,
                                     void* bytes,
                                     uint32_t* num_bytes,
                                     MojoHandle* handles,
                                     uint32_t* num_handles,
                                     MojoReadMessageFlags flags) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->ReadMessage(message_pipe_handle, bytes, num_bytes, handles,
                           num_handles, flags);
}

MojoResult MojoSystemImplCreateDataPipe(
    MojoSystemImpl system,
    const MojoCreateDataPipeOptions* options,
    MojoHandle* data_pipe_producer_handle,
    MojoHandle* data_pipe_consumer_handle) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->CreateDataPipe(options, data_pipe_producer_handle,
                              data_pipe_consumer_handle);
}

MojoResult MojoSystemImplWriteData(MojoSystemImpl system,
                                   MojoHandle data_pipe_producer_handle,
                                   const void* elements,
                                   uint32_t* num_elements,
                                   MojoWriteDataFlags flags) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->WriteData(data_pipe_producer_handle, elements, num_elements,
                         flags);
}

MojoResult MojoSystemImplBeginWriteData(MojoSystemImpl system,
                                        MojoHandle data_pipe_producer_handle,
                                        void** buffer,
                                        uint32_t* buffer_num_elements,
                                        MojoWriteDataFlags flags) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->BeginWriteData(data_pipe_producer_handle, buffer,
                              buffer_num_elements, flags);
}

MojoResult MojoSystemImplEndWriteData(MojoSystemImpl system,
                                      MojoHandle data_pipe_producer_handle,
                                      uint32_t num_elements_written) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->EndWriteData(data_pipe_producer_handle, num_elements_written);
}

MojoResult MojoSystemImplReadData(MojoSystemImpl system,
                                  MojoHandle data_pipe_consumer_handle,
                                  void* elements,
                                  uint32_t* num_elements,
                                  MojoReadDataFlags flags) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->ReadData(data_pipe_consumer_handle, elements, num_elements,
                        flags);
}

MojoResult MojoSystemImplBeginReadData(MojoSystemImpl system,
                                       MojoHandle data_pipe_consumer_handle,
                                       const void** buffer,
                                       uint32_t* buffer_num_elements,
                                       MojoReadDataFlags flags) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->BeginReadData(data_pipe_consumer_handle, buffer,
                             buffer_num_elements, flags);
}

MojoResult MojoSystemImplEndReadData(MojoSystemImpl system,
                                     MojoHandle data_pipe_consumer_handle,
                                     uint32_t num_elements_read) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->EndReadData(data_pipe_consumer_handle, num_elements_read);
}

MojoResult MojoSystemImplCreateSharedBuffer(
    MojoSystemImpl system,
    const MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->CreateSharedBuffer(options, num_bytes, shared_buffer_handle);
}

MojoResult MojoSystemImplDuplicateBufferHandle(
    MojoSystemImpl system,
    MojoHandle buffer_handle,
    const MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->DuplicateBufferHandle(buffer_handle, options, new_buffer_handle);
}

MojoResult MojoSystemImplMapBuffer(MojoSystemImpl system,
                                   MojoHandle buffer_handle,
                                   uint64_t offset,
                                   uint64_t num_bytes,
                                   void** buffer,
                                   MojoMapBufferFlags flags) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->MapBuffer(buffer_handle, offset, num_bytes, buffer, flags);
}

MojoResult MojoSystemImplUnmapBuffer(MojoSystemImpl system, void* buffer) {
  mojo::edk::Core* core = static_cast<mojo::edk::Core*>(system);
  DCHECK(core);
  return core->UnmapBuffer(buffer);
}

}  // extern "C"
*/
