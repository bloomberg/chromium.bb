// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/environment/buffer_tls.h"

#include "mojo/environment/buffer_tls_impl.h"

namespace mojo {
namespace internal {

Buffer* GetCurrentBuffer() {
  return GetCurrentBufferImpl();
}

Buffer* SetCurrentBuffer(Buffer* buf) {
  return SetCurrentBufferImpl(buf);
}

}  // namespace internal
}  // namespace mojo
