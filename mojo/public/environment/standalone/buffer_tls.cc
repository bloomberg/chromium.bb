// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/environment/buffer_tls.h"

#include "mojo/public/environment/standalone/buffer_tls_setup.h"
#include "mojo/public/utility/thread_local.h"

namespace mojo {
namespace internal {

static ThreadLocalPlatform::SlotType s_slot;

void SetUpCurrentBuffer() {
  ThreadLocalPlatform::AllocateSlot(&s_slot);
}

void TearDownCurrentBuffer() {
  ThreadLocalPlatform::FreeSlot(s_slot);
}

Buffer* GetCurrentBuffer() {
  return static_cast<Buffer*>(ThreadLocalPlatform::GetValueFromSlot(s_slot));
}

Buffer* SetCurrentBuffer(Buffer* buf) {
  Buffer* old_buf = GetCurrentBuffer();
  ThreadLocalPlatform::SetValueInSlot(s_slot, buf);
  return old_buf;
}

}  // namespace internal
}  // namespace mojo
