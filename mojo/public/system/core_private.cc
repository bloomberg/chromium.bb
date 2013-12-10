// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/system/core_private.h"

#include <assert.h>
#include <stddef.h>

static mojo::Core* g_core = NULL;

extern "C" {

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

MojoResult MojoCreateMessagePipe(MojoHandle* handle_0, MojoHandle* handle_1) {
  assert(g_core);
  return g_core->CreateMessagePipe(handle_0, handle_1);
}

MojoResult MojoWriteMessage(MojoHandle handle,
                            const void* bytes, uint32_t num_bytes,
                            const MojoHandle* handles, uint32_t num_handles,
                            MojoWriteMessageFlags flags) {
  assert(g_core);
  return g_core->WriteMessage(handle,
                              bytes, num_bytes,
                              handles, num_handles,
                              flags);
}

MojoResult MojoReadMessage(MojoHandle handle,
                           void* bytes, uint32_t* num_bytes,
                           MojoHandle* handles, uint32_t* num_handles,
                           MojoReadMessageFlags flags) {
  assert(g_core);
  return g_core->ReadMessage(handle,
                             bytes, num_bytes,
                             handles, num_handles,
                             flags);
}

MojoTimeTicks MojoGetTimeTicksNow() {
  assert(g_core);
  return g_core->GetTimeTicksNow();
}

}  // extern "C"

namespace mojo {

Core::~Core() {
}

void Core::Init(Core* core) {
  assert(!g_core);
  g_core = core;
}

}  // namespace mojo
