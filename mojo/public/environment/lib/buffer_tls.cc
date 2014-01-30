// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/environment/buffer_tls.h"

#include <assert.h>

#include "mojo/public/environment/lib/buffer_tls_setup.h"
#include "mojo/public/utility/lib/thread_local.h"

namespace mojo {
namespace internal {

static ThreadLocalPointer<Buffer> current_buffer;

void SetUpCurrentBuffer() {
  current_buffer.Allocate();
}

void TearDownCurrentBuffer() {
  assert(!current_buffer.Get());
  current_buffer.Free();
}

Buffer* GetCurrentBuffer() {
  return current_buffer.Get();
}

Buffer* SetCurrentBuffer(Buffer* buf) {
  Buffer* old_buf = current_buffer.Get();
  current_buffer.Set(buf);
  return old_buf;
}

}  // namespace internal
}  // namespace mojo
