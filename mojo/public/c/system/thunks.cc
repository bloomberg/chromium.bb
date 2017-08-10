// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/thunks.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

extern "C" {

static MojoSystemThunks g_thunks = {0};

MojoTimeTicks MojoGetTimeTicksNow() {
  assert(g_thunks.GetTimeTicksNow);
  return g_thunks.GetTimeTicksNow();
}

MojoResult MojoClose(MojoHandle handle) {
  assert(g_thunks.Close);
  return g_thunks.Close(handle);
}

MojoResult MojoQueryHandleSignalsState(
    MojoHandle handle,
    struct MojoHandleSignalsState* signals_state) {
  assert(g_thunks.QueryHandleSignalsState);
  return g_thunks.QueryHandleSignalsState(handle, signals_state);
}

MojoResult MojoCreateMessagePipe(const MojoCreateMessagePipeOptions* options,
                                 MojoHandle* message_pipe_handle0,
                                 MojoHandle* message_pipe_handle1) {
  assert(g_thunks.CreateMessagePipe);
  return g_thunks.CreateMessagePipe(options, message_pipe_handle0,
                                    message_pipe_handle1);
}

MojoResult MojoWriteMessage(MojoHandle message_pipe_handle,
                            MojoMessageHandle message_handle,
                            MojoWriteMessageFlags flags) {
  assert(g_thunks.WriteMessage);
  return g_thunks.WriteMessage(message_pipe_handle, message_handle, flags);
}

MojoResult MojoReadMessage(MojoHandle message_pipe_handle,
                           MojoMessageHandle* message_handle,
                           MojoReadMessageFlags flags) {
  assert(g_thunks.ReadMessage);
  return g_thunks.ReadMessage(message_pipe_handle, message_handle, flags);
}

MojoResult MojoCreateDataPipe(const MojoCreateDataPipeOptions* options,
                              MojoHandle* data_pipe_producer_handle,
                              MojoHandle* data_pipe_consumer_handle) {
  assert(g_thunks.CreateDataPipe);
  return g_thunks.CreateDataPipe(options, data_pipe_producer_handle,
                                 data_pipe_consumer_handle);
}

MojoResult MojoWriteData(MojoHandle data_pipe_producer_handle,
                         const void* elements,
                         uint32_t* num_elements,
                         MojoWriteDataFlags flags) {
  assert(g_thunks.WriteData);
  return g_thunks.WriteData(data_pipe_producer_handle, elements, num_elements,
                            flags);
}

MojoResult MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                              void** buffer,
                              uint32_t* buffer_num_elements,
                              MojoWriteDataFlags flags) {
  assert(g_thunks.BeginWriteData);
  return g_thunks.BeginWriteData(data_pipe_producer_handle, buffer,
                                 buffer_num_elements, flags);
}

MojoResult MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                            uint32_t num_elements_written) {
  assert(g_thunks.EndWriteData);
  return g_thunks.EndWriteData(data_pipe_producer_handle, num_elements_written);
}

MojoResult MojoReadData(MojoHandle data_pipe_consumer_handle,
                        void* elements,
                        uint32_t* num_elements,
                        MojoReadDataFlags flags) {
  assert(g_thunks.ReadData);
  return g_thunks.ReadData(data_pipe_consumer_handle, elements, num_elements,
                           flags);
}

MojoResult MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                             const void** buffer,
                             uint32_t* buffer_num_elements,
                             MojoReadDataFlags flags) {
  assert(g_thunks.BeginReadData);
  return g_thunks.BeginReadData(data_pipe_consumer_handle, buffer,
                                buffer_num_elements, flags);
}

MojoResult MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                           uint32_t num_elements_read) {
  assert(g_thunks.EndReadData);
  return g_thunks.EndReadData(data_pipe_consumer_handle, num_elements_read);
}

MojoResult MojoCreateSharedBuffer(
    const struct MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  assert(g_thunks.CreateSharedBuffer);
  return g_thunks.CreateSharedBuffer(options, num_bytes, shared_buffer_handle);
}

MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  assert(g_thunks.DuplicateBufferHandle);
  return g_thunks.DuplicateBufferHandle(buffer_handle, options,
                                        new_buffer_handle);
}

MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                         uint64_t offset,
                         uint64_t num_bytes,
                         void** buffer,
                         MojoMapBufferFlags flags) {
  assert(g_thunks.MapBuffer);
  return g_thunks.MapBuffer(buffer_handle, offset, num_bytes, buffer, flags);
}

MojoResult MojoUnmapBuffer(void* buffer) {
  assert(g_thunks.UnmapBuffer);
  return g_thunks.UnmapBuffer(buffer);
}

MojoResult MojoCreateWatcher(MojoWatcherCallback callback,
                             MojoHandle* watcher_handle) {
  assert(g_thunks.CreateWatcher);
  return g_thunks.CreateWatcher(callback, watcher_handle);
}

MojoResult MojoWatch(MojoHandle watcher_handle,
                     MojoHandle handle,
                     MojoHandleSignals signals,
                     MojoWatchCondition condition,
                     uintptr_t context) {
  assert(g_thunks.Watch);
  return g_thunks.Watch(watcher_handle, handle, signals, condition, context);
}

MojoResult MojoCancelWatch(MojoHandle watcher_handle, uintptr_t context) {
  assert(g_thunks.CancelWatch);
  return g_thunks.CancelWatch(watcher_handle, context);
}

MojoResult MojoArmWatcher(MojoHandle watcher_handle,
                          uint32_t* num_ready_contexts,
                          uintptr_t* ready_contexts,
                          MojoResult* ready_results,
                          MojoHandleSignalsState* ready_signals_states) {
  assert(g_thunks.ArmWatcher);
  return g_thunks.ArmWatcher(watcher_handle, num_ready_contexts, ready_contexts,
                             ready_results, ready_signals_states);
}

MojoResult MojoFuseMessagePipes(MojoHandle handle0, MojoHandle handle1) {
  assert(g_thunks.FuseMessagePipes);
  return g_thunks.FuseMessagePipes(handle0, handle1);
}

MojoResult MojoCreateMessage(MojoMessageHandle* message) {
  assert(g_thunks.CreateMessage);
  return g_thunks.CreateMessage(message);
}

MojoResult MojoDestroyMessage(MojoMessageHandle message) {
  assert(g_thunks.DestroyMessage);
  return g_thunks.DestroyMessage(message);
}

MojoResult MojoSerializeMessage(MojoMessageHandle message) {
  assert(g_thunks.SerializeMessage);
  return g_thunks.SerializeMessage(message);
}

MojoResult MojoAttachSerializedMessageBuffer(MojoMessageHandle message,
                                             uint32_t payload_size,
                                             const MojoHandle* handles,
                                             uint32_t num_handles,
                                             void** buffer,
                                             uint32_t* buffer_size) {
  assert(g_thunks.AttachSerializedMessageBuffer);
  return g_thunks.AttachSerializedMessageBuffer(
      message, payload_size, handles, num_handles, buffer, buffer_size);
}

MojoResult MojoExtendSerializedMessagePayload(MojoMessageHandle message,
                                              uint32_t new_payload_size,
                                              const MojoHandle* handles,
                                              uint32_t num_handles,
                                              void** buffer,
                                              uint32_t* buffer_size) {
  assert(g_thunks.ExtendSerializedMessagePayload);
  return g_thunks.ExtendSerializedMessagePayload(
      message, new_payload_size, handles, num_handles, buffer, buffer_size);
}

MojoResult MojoCommitSerializedMessageContents(MojoMessageHandle message,
                                               uint32_t final_payload_size,
                                               void** buffer,
                                               uint32_t* buffer_size) {
  assert(g_thunks.CommitSerializedMessageContents);
  return g_thunks.CommitSerializedMessageContents(message, final_payload_size,
                                                  buffer, buffer_size);
}

MojoResult MojoGetSerializedMessageContents(
    MojoMessageHandle message,
    void** buffer,
    uint32_t* num_bytes,
    MojoHandle* handles,
    uint32_t* num_handles,
    MojoGetSerializedMessageContentsFlags flags) {
  assert(g_thunks.GetSerializedMessageContents);
  return g_thunks.GetSerializedMessageContents(message, buffer, num_bytes,
                                               handles, num_handles, flags);
}

MojoResult MojoAttachMessageContext(MojoMessageHandle message,
                                    uintptr_t context,
                                    MojoMessageContextSerializer serializer,
                                    MojoMessageContextDestructor destructor) {
  assert(g_thunks.AttachMessageContext);
  return g_thunks.AttachMessageContext(message, context, serializer,
                                       destructor);
}

MojoResult MojoGetMessageContext(MojoMessageHandle message,
                                 uintptr_t* context,
                                 MojoGetMessageContextFlags flags) {
  assert(g_thunks.GetMessageContext);
  return g_thunks.GetMessageContext(message, context, flags);
}

MojoResult MojoWrapPlatformHandle(
    const struct MojoPlatformHandle* platform_handle,
    MojoHandle* mojo_handle) {
  assert(g_thunks.WrapPlatformHandle);
  return g_thunks.WrapPlatformHandle(platform_handle, mojo_handle);
}

MojoResult MojoUnwrapPlatformHandle(
    MojoHandle mojo_handle,
    struct MojoPlatformHandle* platform_handle) {
  assert(g_thunks.UnwrapPlatformHandle);
  return g_thunks.UnwrapPlatformHandle(mojo_handle, platform_handle);
}

MojoResult MojoWrapPlatformSharedBufferHandle(
    const struct MojoPlatformHandle* platform_handle,
    size_t num_bytes,
    const struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedBufferHandleFlags flags,
    MojoHandle* mojo_handle) {
  assert(g_thunks.WrapPlatformSharedBufferHandle);
  return g_thunks.WrapPlatformSharedBufferHandle(platform_handle, num_bytes,
                                                 guid, flags, mojo_handle);
}

MojoResult MojoUnwrapPlatformSharedBufferHandle(
    MojoHandle mojo_handle,
    struct MojoPlatformHandle* platform_handle,
    size_t* num_bytes,
    struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedBufferHandleFlags* flags) {
  assert(g_thunks.UnwrapPlatformSharedBufferHandle);
  return g_thunks.UnwrapPlatformSharedBufferHandle(mojo_handle, platform_handle,
                                                   num_bytes, guid, flags);
}

MojoResult MojoNotifyBadMessage(MojoMessageHandle message,
                                const char* error,
                                size_t error_num_bytes) {
  assert(g_thunks.NotifyBadMessage);
  return g_thunks.NotifyBadMessage(message, error, error_num_bytes);
}

MojoResult MojoGetProperty(MojoPropertyType type, void* value) {
  assert(g_thunks.GetProperty);
  return g_thunks.GetProperty(type, value);
}

}  // extern "C"

size_t MojoEmbedderSetSystemThunks(const MojoSystemThunks* system_thunks) {
  if (system_thunks->size >= sizeof(g_thunks))
    g_thunks = *system_thunks;
  return sizeof(g_thunks);
}
