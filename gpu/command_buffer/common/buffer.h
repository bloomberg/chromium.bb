// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_BUFFER_H_

namespace base {
  class SharedMemory;
}

namespace gpu {

// Address and size of a buffer and optionally a shared memory object. This
// type has value semantics.
struct Buffer {
  Buffer() : ptr(NULL), size(0), shared_memory(NULL) {
  }

  void* ptr;
  size_t size;

  // Null if the buffer is not shared memory or if it is not exposed as such.
  base::SharedMemory* shared_memory;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_BUFFER_H_
