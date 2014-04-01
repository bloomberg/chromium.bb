// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/buffer.h"

#include <assert.h>

#include "mojo/public/cpp/environment/buffer_tls.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

Buffer::Buffer() {
  previous_ = internal::SetCurrentBuffer(this);
}

Buffer::~Buffer() {
  Buffer* buf MOJO_ALLOW_UNUSED = internal::SetCurrentBuffer(previous_);
  assert(buf == this);
}

Buffer* Buffer::current() {
  return internal::GetCurrentBuffer();
}

}  // namespace mojo
