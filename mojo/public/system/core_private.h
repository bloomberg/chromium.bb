// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_CORE_PRIVATE_H_
#define MOJO_PUBLIC_SYSTEM_CORE_PRIVATE_H_

#include "mojo/public/system/core.h"

namespace mojo {

// Implementors of the core APIs can use this interface to install their
// implementation into the mojo_system dynamic library. Mojo clients should not
// call these functions directly.
class MOJO_SYSTEM_EXPORT CorePrivate {
 public:
  virtual ~CorePrivate();

  static void Init(CorePrivate* core);

  virtual MojoTimeTicks GetTimeTicksNow() = 0;
  virtual MojoResult Close(MojoHandle handle) = 0;
  virtual MojoResult Wait(MojoHandle handle,
                          MojoWaitFlags flags,
                          MojoDeadline deadline) = 0;
  virtual MojoResult WaitMany(const MojoHandle* handles,
                              const MojoWaitFlags* flags,
                              uint32_t num_handles,
                              MojoDeadline deadline) = 0;
  virtual MojoResult CreateMessagePipe(MojoHandle* message_pipe_handle_0,
                                       MojoHandle* message_pipe_handle_1) = 0;
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
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SYSTEM_CORE_PRIVATE_H_
