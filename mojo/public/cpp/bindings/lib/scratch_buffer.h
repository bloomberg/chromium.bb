// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SCRATCH_BUFFER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SCRATCH_BUFFER_H_

#include <deque>

#include "mojo/public/cpp/bindings/buffer.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace internal {

// The following class is designed to be allocated on the stack.  If necessary,
// it will failover to allocating objects on the heap.
class ScratchBuffer : public Buffer {
 public:
  ScratchBuffer();
  virtual ~ScratchBuffer();

  virtual void* Allocate(size_t num_bytes, Destructor func = NULL)
      MOJO_OVERRIDE;

 private:
  enum {
    kMinSegmentSize = 512,
    kMaxSegmentSize = 1024 * 1024 * 1024,
  };

  struct Segment {
    Segment* next;
    char* cursor;
    char* end;
  };

  void* AllocateInSegment(Segment* segment, size_t num_bytes);
  bool AddOverflowSegment(size_t delta);

  char fixed_data_[kMinSegmentSize];
  Segment fixed_;
  Segment* overflow_;

  struct PendingDestructor {
    Destructor func;
    void* address;
  };
  std::deque<PendingDestructor> pending_dtors_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScratchBuffer);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SCRATCH_BUFFER_H_
