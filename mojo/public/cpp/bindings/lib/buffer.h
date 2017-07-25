// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BUFFER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/bindings_export.h"

namespace mojo {
namespace internal {

// Buffer provides an interface to allocate memory blocks which are 8-byte
// aligned. It doesn't own the underlying memory. Users must ensure that the
// memory stays valid while using the allocated blocks from Buffer.
//
// A Buffer may be moved around. A moved-from Buffer is reset and may no longer
// be used to Allocate memory unless re-Initialized.
class MOJO_CPP_BINDINGS_EXPORT Buffer {
 public:
  // Constructs an invalid Buffer. May not call Allocate().
  Buffer();

  // Constructs a Buffer which can Allocate() blocks starting at |data|, up to
  // a total of |size| bytes. |data| is not owned.
  Buffer(void* data, size_t size);

  Buffer(Buffer&& other);
  ~Buffer();

  Buffer& operator=(Buffer&& other);

  void* data() const { return data_; }
  size_t size() const { return size_; }
  size_t cursor() const { return cursor_; }

  bool is_valid() const { return data_ != nullptr; }

  // Allocates |num_bytes| from the buffer and returns a pointer to the start of
  // the allocated block. The resulting address is 8-byte aligned.
  void* Allocate(size_t num_bytes);

  // Resets the buffer to an invalid state. Can no longer be used to Allocate().
  void Reset();

 private:
  void* data_ = nullptr;
  size_t size_ = 0;
  size_t cursor_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BUFFER_H_
