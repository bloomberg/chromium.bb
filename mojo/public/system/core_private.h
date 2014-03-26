// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_CORE_PRIVATE_H_
#define MOJO_PUBLIC_SYSTEM_CORE_PRIVATE_H_

#include "mojo/public/c/system/core.h"

namespace mojo {

// Implementors of the core APIs can use this interface to install their
// implementation into the mojo_system dynamic library. Mojo clients should not
// call these functions directly.
class MOJO_SYSTEM_EXPORT Core {
 public:
  virtual ~Core();

  static void Init(Core* core);
  static Core* Get();
  static void Reset();

  virtual MojoTimeTicks GetTimeTicksNow() = 0;
  virtual MojoResult Close(MojoHandle handle) = 0;
  virtual MojoResult Wait(MojoHandle handle,
                          MojoWaitFlags flags,
                          MojoDeadline deadline) = 0;
  virtual MojoResult WaitMany(const MojoHandle* handles,
                              const MojoWaitFlags* flags,
                              uint32_t num_handles,
                              MojoDeadline deadline) = 0;
  virtual MojoResult CreateMessagePipe(MojoHandle* message_pipe_handle0,
                                       MojoHandle* message_pipe_handle1) = 0;
  virtual MojoResult WriteMessage(MojoHandle message_pipe_handle,
                                  const void* bytes,
                                  uint32_t num_bytes,
                                  const MojoHandle* handles,
                                  uint32_t num_handles,
                                  MojoWriteMessageFlags flags) = 0;
  virtual MojoResult ReadMessage(MojoHandle message_pipe_handle,
                                 void* bytes,
                                 uint32_t* num_bytes,
                                 MojoHandle* handles,
                                 uint32_t* num_handles,
                                 MojoReadMessageFlags flags) = 0;
  virtual MojoResult CreateDataPipe(const MojoCreateDataPipeOptions* options,
                                    MojoHandle* data_pipe_producer_handle,
                                    MojoHandle* data_pipe_consumer_handle) = 0;
  virtual MojoResult WriteData(MojoHandle data_pipe_producer_handle,
                               const void* elements,
                               uint32_t* num_elements,
                               MojoWriteDataFlags flags) = 0;
  virtual MojoResult BeginWriteData(MojoHandle data_pipe_producer_handle,
                                    void** buffer,
                                    uint32_t* buffer_num_elements,
                                    MojoWriteDataFlags flags) = 0;
  virtual MojoResult EndWriteData(MojoHandle data_pipe_producer_handle,
                                  uint32_t num_elements_written) = 0;
  virtual MojoResult ReadData(MojoHandle data_pipe_consumer_handle,
                              void* elements,
                              uint32_t* num_elements,
                              MojoReadDataFlags flags) = 0;
  virtual MojoResult BeginReadData(MojoHandle data_pipe_consumer_handle,
                                   const void** buffer,
                                   uint32_t* buffer_num_elements,
                                   MojoReadDataFlags flags) = 0;
  virtual MojoResult EndReadData(MojoHandle data_pipe_consumer_handle,
                                 uint32_t num_elements_read) = 0;
  virtual MojoResult CreateSharedBuffer(
      const MojoCreateSharedBufferOptions* options,
      uint64_t num_bytes,
      MojoHandle* shared_buffer_handle) = 0;
  virtual MojoResult DuplicateBufferHandle(
      MojoHandle buffer_handle,
      const MojoDuplicateBufferHandleOptions* options,
      MojoHandle* new_buffer_handle) = 0;
  virtual MojoResult MapBuffer(MojoHandle buffer_handle,
                               uint64_t offset,
                               uint64_t num_bytes,
                               void** buffer,
                               MojoMapBufferFlags flags) = 0;
  virtual MojoResult UnmapBuffer(void* buffer) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SYSTEM_CORE_PRIVATE_H_
