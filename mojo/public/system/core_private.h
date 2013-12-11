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

  virtual MojoResult Close(MojoHandle handle) = 0;
  virtual MojoResult Wait(MojoHandle handle,
                          MojoWaitFlags flags,
                          MojoDeadline deadline) = 0;
  virtual MojoResult WaitMany(const MojoHandle* handles,
                              const MojoWaitFlags* flags,
                              uint32_t num_handles,
                              MojoDeadline deadline) = 0;
  virtual MojoResult CreateMessagePipe(MojoHandle* handle_0,
                                       MojoHandle* handle_1) = 0;
  virtual MojoResult WriteMessage(MojoHandle handle,
                                  const void* bytes,
                                  uint32_t num_bytes,
                                  const MojoHandle* handles,
                                  uint32_t num_handles,
                                  MojoWriteMessageFlags flags) = 0;
  virtual MojoResult ReadMessage(MojoHandle handle,
                                 void* bytes,
                                 uint32_t* num_bytes,
                                 MojoHandle* handles,
                                 uint32_t* num_handles,
                                 MojoReadMessageFlags flags) = 0;
  virtual MojoTimeTicks GetTimeTicksNow() = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SYSTEM_CORE_PRIVATE_H_
