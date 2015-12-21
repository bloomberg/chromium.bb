// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_PICKLE_BUFFER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_PICKLE_BUFFER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"

namespace mojo {
namespace internal {

// An implementation of Buffer which uses base::Pickle for its backing. Note
// that this does not use Pickle's header structure at all, instead storing
// the complete Message (including header) in the Pickle's payload.
class PickleBuffer : public Buffer {
 public:
  PickleBuffer(size_t num_bytes, bool zero_initialized);
  ~PickleBuffer() override;

  const void* data() const;

  void* data() {
    return const_cast<void*>(static_cast<const PickleBuffer*>(this)->data());
  }

  size_t data_num_bytes() const;

  base::Pickle* pickle() const;

 private:
  class Storage;

  // Buffer implementation. Note that this cannot grow the Pickle's capacity and
  // it is an error to Allocate() more bytes in total than have been
  // pre-allocated using ReserveCapacity() or ReserveUninitializedCapacity().
  //
  // This guarantees that the returned data is aligned on an 8-byte boundary.
  void* Allocate(size_t num_bytes) override;
  PickleBuffer* AsPickleBuffer() override;

  scoped_ptr<Storage> storage_;

  DISALLOW_COPY_AND_ASSIGN(PickleBuffer);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_PICKLE_BUFFER_H_
