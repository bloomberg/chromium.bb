// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/entrypoints.h"

#include <stdint.h>

#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/platform_handle.h"

using mojo::edk::internal::g_core;

// Definitions of the system functions.
extern "C" {

MojoTimeTicks MojoGetTimeTicksNowImpl() {
  return g_core->GetTimeTicksNow();
}

MojoResult MojoCloseImpl(MojoHandle handle) {
  return g_core->Close(handle);
}

MojoResult MojoQueryHandleSignalsStateImpl(
    MojoHandle handle,
    MojoHandleSignalsState* signals_state) {
  return g_core->QueryHandleSignalsState(handle, signals_state);
}

MojoResult MojoCreateWatcherImpl(MojoWatcherCallback callback,
                                 MojoHandle* watcher_handle) {
  return g_core->CreateWatcher(callback, watcher_handle);
}

MojoResult MojoArmWatcherImpl(MojoHandle watcher_handle,
                              uint32_t* num_ready_contexts,
                              uintptr_t* ready_contexts,
                              MojoResult* ready_results,
                              MojoHandleSignalsState* ready_signals_states) {
  return g_core->ArmWatcher(watcher_handle, num_ready_contexts, ready_contexts,
                            ready_results, ready_signals_states);
}

MojoResult MojoWatchImpl(MojoHandle watcher_handle,
                         MojoHandle handle,
                         MojoHandleSignals signals,
                         uintptr_t context) {
  return g_core->Watch(watcher_handle, handle, signals, context);
}

MojoResult MojoCancelWatchImpl(MojoHandle watcher_handle, uintptr_t context) {
  return g_core->CancelWatch(watcher_handle, context);
}

MojoResult MojoCreateMessageImpl(uintptr_t context,
                                 const MojoMessageOperationThunks* thunks,
                                 MojoMessageHandle* message) {
  return g_core->CreateMessage(context, thunks, message);
}

MojoResult MojoDestroyMessageImpl(MojoMessageHandle message) {
  return g_core->DestroyMessage(message);
}

MojoResult MojoSerializeMessageImpl(MojoMessageHandle message) {
  return g_core->SerializeMessage(message);
}

MojoResult MojoGetSerializedMessageContentsImpl(
    MojoMessageHandle message,
    void** buffer,
    uint32_t* num_bytes,
    MojoHandle* handles,
    uint32_t* num_handles,
    MojoGetSerializedMessageContentsFlags flags) {
  return g_core->GetSerializedMessageContents(message, buffer, num_bytes,
                                              handles, num_handles, flags);
}

MojoResult MojoReleaseMessageContextImpl(MojoMessageHandle message,
                                         uintptr_t* context) {
  return g_core->ReleaseMessageContext(message, context);
}

MojoResult MojoCreateMessagePipeImpl(
    const MojoCreateMessagePipeOptions* options,
    MojoHandle* message_pipe_handle0,
    MojoHandle* message_pipe_handle1) {
  return g_core->CreateMessagePipe(options, message_pipe_handle0,
                                   message_pipe_handle1);
}

MojoResult MojoWriteMessageImpl(MojoHandle message_pipe_handle,
                                MojoMessageHandle message,
                                MojoWriteMessageFlags flags) {
  return g_core->WriteMessage(message_pipe_handle, message, flags);
}

MojoResult MojoReadMessageImpl(MojoHandle message_pipe_handle,
                               MojoMessageHandle* message,
                               MojoReadMessageFlags flags) {
  return g_core->ReadMessage(message_pipe_handle, message, flags);
}

MojoResult MojoFuseMessagePipesImpl(MojoHandle handle0, MojoHandle handle1) {
  return g_core->FuseMessagePipes(handle0, handle1);
}

MojoResult MojoCreateDataPipeImpl(const MojoCreateDataPipeOptions* options,
                                  MojoHandle* data_pipe_producer_handle,
                                  MojoHandle* data_pipe_consumer_handle) {
  return g_core->CreateDataPipe(options, data_pipe_producer_handle,
                                data_pipe_consumer_handle);
}

MojoResult MojoWriteDataImpl(MojoHandle data_pipe_producer_handle,
                             const void* elements,
                             uint32_t* num_elements,
                             MojoWriteDataFlags flags) {
  return g_core->WriteData(data_pipe_producer_handle, elements, num_elements,
                           flags);
}

MojoResult MojoBeginWriteDataImpl(MojoHandle data_pipe_producer_handle,
                                  void** buffer,
                                  uint32_t* buffer_num_elements,
                                  MojoWriteDataFlags flags) {
  return g_core->BeginWriteData(data_pipe_producer_handle, buffer,
                                buffer_num_elements, flags);
}

MojoResult MojoEndWriteDataImpl(MojoHandle data_pipe_producer_handle,
                                uint32_t num_elements_written) {
  return g_core->EndWriteData(data_pipe_producer_handle, num_elements_written);
}

MojoResult MojoReadDataImpl(MojoHandle data_pipe_consumer_handle,
                            void* elements,
                            uint32_t* num_elements,
                            MojoReadDataFlags flags) {
  return g_core->ReadData(data_pipe_consumer_handle, elements, num_elements,
                          flags);
}

MojoResult MojoBeginReadDataImpl(MojoHandle data_pipe_consumer_handle,
                                 const void** buffer,
                                 uint32_t* buffer_num_elements,
                                 MojoReadDataFlags flags) {
  return g_core->BeginReadData(data_pipe_consumer_handle, buffer,
                               buffer_num_elements, flags);
}

MojoResult MojoEndReadDataImpl(MojoHandle data_pipe_consumer_handle,
                               uint32_t num_elements_read) {
  return g_core->EndReadData(data_pipe_consumer_handle, num_elements_read);
}

MojoResult MojoCreateSharedBufferImpl(
    const struct MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  return g_core->CreateSharedBuffer(options, num_bytes, shared_buffer_handle);
}

MojoResult MojoDuplicateBufferHandleImpl(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  return g_core->DuplicateBufferHandle(buffer_handle, options,
                                       new_buffer_handle);
}

MojoResult MojoMapBufferImpl(MojoHandle buffer_handle,
                             uint64_t offset,
                             uint64_t num_bytes,
                             void** buffer,
                             MojoMapBufferFlags flags) {
  return g_core->MapBuffer(buffer_handle, offset, num_bytes, buffer, flags);
}

MojoResult MojoUnmapBufferImpl(void* buffer) {
  return g_core->UnmapBuffer(buffer);
}

MojoResult MojoWrapPlatformHandleImpl(const MojoPlatformHandle* platform_handle,
                                      MojoHandle* mojo_handle) {
  return g_core->WrapPlatformHandle(platform_handle, mojo_handle);
}

MojoResult MojoUnwrapPlatformHandleImpl(MojoHandle mojo_handle,
                                        MojoPlatformHandle* platform_handle) {
  return g_core->UnwrapPlatformHandle(mojo_handle, platform_handle);
}

MojoResult MojoWrapPlatformSharedBufferHandleImpl(
    const MojoPlatformHandle* platform_handle,
    size_t num_bytes,
    MojoPlatformSharedBufferHandleFlags flags,
    MojoHandle* mojo_handle) {
  return g_core->WrapPlatformSharedBufferHandle(platform_handle, num_bytes,
                                                flags, mojo_handle);
}

MojoResult MojoUnwrapPlatformSharedBufferHandleImpl(
    MojoHandle mojo_handle,
    MojoPlatformHandle* platform_handle,
    size_t* num_bytes,
    MojoPlatformSharedBufferHandleFlags* flags) {
  return g_core->UnwrapPlatformSharedBufferHandle(mojo_handle, platform_handle,
                                                  num_bytes, flags);
}

MojoResult MojoNotifyBadMessageImpl(MojoMessageHandle message,
                                    const char* error,
                                    size_t error_num_bytes) {
  return g_core->NotifyBadMessage(message, error, error_num_bytes);
}

MojoResult MojoGetPropertyImpl(MojoPropertyType type, void* value) {
  return g_core->GetProperty(type, value);
}

}  // extern "C"

namespace mojo {
namespace edk {

MojoSystemThunks MakeSystemThunks() {
  MojoSystemThunks system_thunks = {sizeof(MojoSystemThunks),
                                    MojoGetTimeTicksNowImpl,
                                    MojoCloseImpl,
                                    MojoQueryHandleSignalsStateImpl,
                                    MojoCreateMessagePipeImpl,
                                    MojoWriteMessageImpl,
                                    MojoReadMessageImpl,
                                    MojoCreateDataPipeImpl,
                                    MojoWriteDataImpl,
                                    MojoBeginWriteDataImpl,
                                    MojoEndWriteDataImpl,
                                    MojoReadDataImpl,
                                    MojoBeginReadDataImpl,
                                    MojoEndReadDataImpl,
                                    MojoCreateSharedBufferImpl,
                                    MojoDuplicateBufferHandleImpl,
                                    MojoMapBufferImpl,
                                    MojoUnmapBufferImpl,
                                    MojoCreateWatcherImpl,
                                    MojoWatchImpl,
                                    MojoCancelWatchImpl,
                                    MojoArmWatcherImpl,
                                    MojoFuseMessagePipesImpl,
                                    MojoCreateMessageImpl,
                                    MojoDestroyMessageImpl,
                                    MojoSerializeMessageImpl,
                                    MojoGetSerializedMessageContentsImpl,
                                    MojoReleaseMessageContextImpl,
                                    MojoWrapPlatformHandleImpl,
                                    MojoUnwrapPlatformHandleImpl,
                                    MojoWrapPlatformSharedBufferHandleImpl,
                                    MojoUnwrapPlatformSharedBufferHandleImpl,
                                    MojoNotifyBadMessageImpl,
                                    MojoGetPropertyImpl};
  return system_thunks;
}

}  // namespace edk
}  // namespace mojo
