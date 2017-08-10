// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_THUNKS_H_
#define MOJO_PUBLIC_C_SYSTEM_THUNKS_H_

#include <stddef.h>
#include <stdint.h>

#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/system_export.h"

// Structure used to bind the basic Mojo Core functions to an embedder
// implementation. This is intended to eventually be used as a stable ABI
// between a Mojo embedder and some loaded application code, but for now it is
// still effectively safe to rearrange entries as needed.
#pragma pack(push, 8)
struct MojoSystemThunks {
  size_t size;  // Should be set to sizeof(MojoSystemThunks).
  MojoTimeTicks (*GetTimeTicksNow)();
  MojoResult (*Close)(MojoHandle handle);
  MojoResult (*QueryHandleSignalsState)(
      MojoHandle handle,
      struct MojoHandleSignalsState* signals_state);
  MojoResult (*CreateMessagePipe)(
      const struct MojoCreateMessagePipeOptions* options,
      MojoHandle* message_pipe_handle0,
      MojoHandle* message_pipe_handle1);
  MojoResult (*WriteMessage)(MojoHandle message_pipe_handle,
                             MojoMessageHandle message_handle,
                             MojoWriteMessageFlags flags);
  MojoResult (*ReadMessage)(MojoHandle message_pipe_handle,
                            MojoMessageHandle* message_handle,
                            MojoReadMessageFlags flags);
  MojoResult (*CreateDataPipe)(const struct MojoCreateDataPipeOptions* options,
                               MojoHandle* data_pipe_producer_handle,
                               MojoHandle* data_pipe_consumer_handle);
  MojoResult (*WriteData)(MojoHandle data_pipe_producer_handle,
                          const void* elements,
                          uint32_t* num_elements,
                          MojoWriteDataFlags flags);
  MojoResult (*BeginWriteData)(MojoHandle data_pipe_producer_handle,
                               void** buffer,
                               uint32_t* buffer_num_elements,
                               MojoWriteDataFlags flags);
  MojoResult (*EndWriteData)(MojoHandle data_pipe_producer_handle,
                             uint32_t num_elements_written);
  MojoResult (*ReadData)(MojoHandle data_pipe_consumer_handle,
                         void* elements,
                         uint32_t* num_elements,
                         MojoReadDataFlags flags);
  MojoResult (*BeginReadData)(MojoHandle data_pipe_consumer_handle,
                              const void** buffer,
                              uint32_t* buffer_num_elements,
                              MojoReadDataFlags flags);
  MojoResult (*EndReadData)(MojoHandle data_pipe_consumer_handle,
                            uint32_t num_elements_read);
  MojoResult (*CreateSharedBuffer)(
      const struct MojoCreateSharedBufferOptions* options,
      uint64_t num_bytes,
      MojoHandle* shared_buffer_handle);
  MojoResult (*DuplicateBufferHandle)(
      MojoHandle buffer_handle,
      const struct MojoDuplicateBufferHandleOptions* options,
      MojoHandle* new_buffer_handle);
  MojoResult (*MapBuffer)(MojoHandle buffer_handle,
                          uint64_t offset,
                          uint64_t num_bytes,
                          void** buffer,
                          MojoMapBufferFlags flags);
  MojoResult (*UnmapBuffer)(void* buffer);
  MojoResult (*CreateWatcher)(MojoWatcherCallback callback,
                              MojoHandle* watcher_handle);
  MojoResult (*Watch)(MojoHandle watcher_handle,
                      MojoHandle handle,
                      MojoHandleSignals signals,
                      MojoWatchCondition condition,
                      uintptr_t context);
  MojoResult (*CancelWatch)(MojoHandle watcher_handle, uintptr_t context);
  MojoResult (*ArmWatcher)(MojoHandle watcher_handle,
                           uint32_t* num_ready_contexts,
                           uintptr_t* ready_contexts,
                           MojoResult* ready_results,
                           MojoHandleSignalsState* ready_signals_states);
  MojoResult (*FuseMessagePipes)(MojoHandle handle0, MojoHandle handle1);
  MojoResult (*CreateMessage)(MojoMessageHandle* message);
  MojoResult (*DestroyMessage)(MojoMessageHandle message);
  MojoResult (*SerializeMessage)(MojoMessageHandle message);
  MojoResult (*AttachSerializedMessageBuffer)(MojoMessageHandle message,
                                              uint32_t payload_size,
                                              const MojoHandle* handles,
                                              uint32_t num_handles,
                                              void** buffer,
                                              uint32_t* buffer_size);
  MojoResult (*ExtendSerializedMessagePayload)(MojoMessageHandle message,
                                               uint32_t new_payload_size,
                                               const MojoHandle* handles,
                                               uint32_t num_handles,
                                               void** buffer,
                                               uint32_t* buffer_size);
  MojoResult (*CommitSerializedMessageContents)(MojoMessageHandle message,
                                                uint32_t final_payload_size,
                                                void** buffer,
                                                uint32_t* buffer_size);
  MojoResult (*GetSerializedMessageContents)(
      MojoMessageHandle message,
      void** buffer,
      uint32_t* num_bytes,
      MojoHandle* handles,
      uint32_t* num_handles,
      MojoGetSerializedMessageContentsFlags flags);
  MojoResult (*AttachMessageContext)(MojoMessageHandle message,
                                     uintptr_t context,
                                     MojoMessageContextSerializer serializer,
                                     MojoMessageContextDestructor destructor);
  MojoResult (*GetMessageContext)(MojoMessageHandle message,
                                  uintptr_t* context,
                                  MojoGetMessageContextFlags flags);
  MojoResult (*WrapPlatformHandle)(
      const struct MojoPlatformHandle* platform_handle,
      MojoHandle* mojo_handle);
  MojoResult (*UnwrapPlatformHandle)(
      MojoHandle mojo_handle,
      struct MojoPlatformHandle* platform_handle);
  MojoResult (*WrapPlatformSharedBufferHandle)(
      const struct MojoPlatformHandle* platform_handle,
      size_t num_bytes,
      const struct MojoSharedBufferGuid* guid,
      MojoPlatformSharedBufferHandleFlags flags,
      MojoHandle* mojo_handle);
  MojoResult (*UnwrapPlatformSharedBufferHandle)(
      MojoHandle mojo_handle,
      struct MojoPlatformHandle* platform_handle,
      size_t* num_bytes,
      struct MojoSharedBufferGuid* guid,
      MojoPlatformSharedBufferHandleFlags* flags);
  MojoResult (*NotifyBadMessage)(MojoMessageHandle message,
                                 const char* error,
                                 size_t error_num_bytes);
  MojoResult (*GetProperty)(MojoPropertyType type, void* value);
};
#pragma pack(pop)

// Use this type for the function found by dynamically discovering it in
// a DSO linked with mojo_system. For example:
// MojoSetSystemThunksFn mojo_set_system_thunks_fn =
//     reinterpret_cast<MojoSetSystemThunksFn>(app_library.GetFunctionPointer(
//         "MojoSetSystemThunks"));
// The expected size of |system_thunks| is returned.
// The contents of |system_thunks| are copied.
typedef size_t (*MojoSetSystemThunksFn)(
    const struct MojoSystemThunks* system_thunks);

// A function for setting up the embedder's own system thunks. This should only
// be called by Mojo embedder code.
MOJO_SYSTEM_EXPORT size_t
MojoEmbedderSetSystemThunks(const struct MojoSystemThunks* system_thunks);

#endif  // MOJO_PUBLIC_C_SYSTEM_THUNKS_H_
