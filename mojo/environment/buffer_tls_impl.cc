// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/environment/buffer_tls_impl.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"

namespace mojo {
namespace internal {
namespace {

base::LazyInstance<base::ThreadLocalPointer<Buffer> >::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

Buffer* GetCurrentBufferImpl() {
  return lazy_tls_ptr.Pointer()->Get();
}

Buffer* SetCurrentBufferImpl(Buffer* buf) {
  Buffer* old_buf = lazy_tls_ptr.Pointer()->Get();
  lazy_tls_ptr.Pointer()->Set(buf);
  return old_buf;
}

}  // namespace internal
}  // namespace mojo
