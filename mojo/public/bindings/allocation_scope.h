// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_ALLOCATION_SCOPE_H_
#define MOJO_PUBLIC_BINDINGS_ALLOCATION_SCOPE_H_

#include "mojo/public/bindings/lib/scratch_buffer.h"
#include "mojo/public/system/macros.h"

namespace mojo {

class AllocationScope {
 public:
  AllocationScope() {}
  ~AllocationScope() {}

  Buffer* buffer() { return &buffer_; }

 private:
  internal::ScratchBuffer buffer_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(AllocationScope);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_ALLOCATION_SCOPE_H_
