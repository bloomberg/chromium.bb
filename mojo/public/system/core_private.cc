// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/system/core_private.h"

#include <assert.h>
#include <stddef.h>

static mojo::CorePrivate* g_core = NULL;

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

MojoResult MojoCreateMessagePipe(MojoHandle* message_pipe_handle_0,
                                 MojoHandle* message_pipe_handle_1) {
  assert(g_core);
  return g_core->CreateMessagePipe(message_pipe_handle_0,
                                   message_pipe_handle_1);
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

}  // extern "C"

namespace mojo {

CorePrivate::~CorePrivate() {
}

void CorePrivate::Init(CorePrivate* core) {
  assert(!g_core);
  g_core = core;
}

}  // namespace mojo
